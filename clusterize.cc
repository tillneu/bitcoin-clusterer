#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <list>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdint>

#include "hdr/data.h"


using namespace std;




addr2info_t 		addr2info;
list<cluster_s> 	clusterlist;	// must not be vector! (resize --> copy --> pointers to elements are wrong
char* 				bufbegin;	// begin of tx graph buffer


void readTxCount(const string fname) {
	ifstream file(fname, ios::binary | std::ios::ate);

	size_t entrysize = ADDRBYTES + sizeof(uint32_t);
	size_t nentries = file.tellg() / entrysize;
	file.seekg(0, std::ios::beg);

	addr_t tmpaddr;

	unsigned char addr[ADDRBYTES];
	uint32_t count;
	cout << "reading " << fname << " start. Total Entries: " << nentries << endl;
	addr2info.reserve(nentries);
	while (file.read((char*)&addr, ADDRBYTES)) {
		if (addr2info.size() % 1000000 == 0) {
			cout << "reading " << fname << " : " << addr2info.size()
					<< " --> " << addr2info.size()/(double)nentries*100.0 << " %"
					<<  "\tmemory: " << memory_used () << endl;
		}
		file.read((char*)&count, sizeof(uint32_t));

		LOG(EncodeBase58(addr, addr+ADDRBYTES) << " --> " << count << endl);

		tmpaddr.assign(addr, addr+ADDRBYTES);
		addr2info.emplace(piecewise_construct, forward_as_tuple(tmpaddr), forward_as_tuple((cluster_s*)NULL, count, 0, false));
	}

	cout << "reading " << fname << " finished" << endl;
}


/*
 * merges the addresses of two clusters into one new cluster
 * and returns a pointer to that new cluster
 */
cluster_s* mergeClusters(cluster_s* ca, cluster_s* cb) {
	if (ca == cb) {
		return ca;
	}
	if (ca->addresses.size() < cb->addresses.size()) {
		return mergeClusters(cb, ca);
	}
	//LOG("merge clusters, smaller: " << cb->size( ) << endl);
	// ca is the bigger cluster --> append cb to ca

	ca->addresses.insert(ca->addresses.end(), cb->addresses.begin(), cb->addresses.end());
	ca->txes.insert(ca->txes.end(), cb->txes.begin(), cb->txes.end());

	addr_t tmpaddr;
	for (const unsigned char* addr : cb->addresses) {
		tmpaddr.assign(addr, addr+ADDRBYTES);
		auto it = addr2info.find(tmpaddr);
		if (it == addr2info.end()) {
			cerr << "mergeClusters::  address not in addr2info: " << EncodeBase58(addr, addr+ADDRBYTES) << endl;
			continue;
		}
		it->second.cluster = ca;
	}
	cb->addresses.clear();
	cb->txes.clear();

	return ca;
}


/*
 * curCluster contains new addresses that are in one cluster
 * this changes addr2info so that all addresses point to the same cluster
 * this also transitively merges clusters
 */
void updateClusters(tx_s* tx, const addrlist_t& curAddrList) {
	static addr_t tmpaddr;
	cluster_s* tmpcluster = NULL;
	for (const auto addr : curAddrList) {
		tmpaddr.assign(addr, addr+ADDRBYTES);
		auto it = addr2info.find(tmpaddr);
		if (it == addr2info.end()) {
			cerr << "updateClusters:  address not in addr2info: " << EncodeBase58(addr, addr+ADDRBYTES) << endl;
			continue;
		}
		if (tmpcluster == NULL) {
			if (it->second.cluster == NULL) {
				// create new cluster
				clusterlist.push_back({});
				tmpcluster = &clusterlist.back();
				it->second.cluster= tmpcluster;
				tmpcluster->addresses.push_back(addr);
			} else {
				// make this cluster the cluster to add addresses to
				tmpcluster = it->second.cluster;
			}
		} else {
			if (it->second.cluster == NULL) {
				// addr is in no cluster yet, add addr to tmpcluster
				it->second.cluster = tmpcluster;
				tmpcluster->addresses.push_back(addr);
			} else {
				// addr already is in cluster, merge clusters
				tmpcluster = mergeClusters(tmpcluster, it->second.cluster);
			}
		}
	}
	if (tmpcluster != NULL) {
		tmpcluster->txes.push_back(tx);
	}

}


/*
 * multi input heuristic, all inputs are in one cluster
 */
void H1(addrlist_t& curAddrList, tx_s* tx, output_s outputs[], size_t offsets[]) {
	assert(curAddrList.size() == 0);
	curAddrList.resize(tx->nInput);

	for (size_t i = 0; i < tx->nInput; ++i) {

		output_s* refoutput = (output_s*)(bufbegin + offsets[i]);
		tx_s* reftx = (tx_s*)(bufbegin+refoutput->txoffs);

		LOG("\tInput from: " << EncodeBase58(refoutput->addr, refoutput->addr+ADDRBYTES)
				<< " TX = "<< Hash2String(reftx->hash) << endl);

		curAddrList[i] = refoutput->addr;
	}

	//LOG("H1 " << curCluster.size() << endl);
}



/*
 * change address
 */
bool H2(addrlist_t& curAddrList, tx_s* tx, output_s outputs[], size_t offsets[],
		bool h2refinedA, bool h2refinedB, bool h2refinedC) {
	static addr_t tmpaddr;

	//cout << "h2" << endl;

	// set to true if a condition is met that prevents any change address from being selected
	// (we can't return directly)
	bool blocked = false;

	/*
	 * Transaction is Coinbase or has only one output
	 */
	if (tx->nInput == 0 || tx->nOutput < 2) {
		return false;
	}

	unsigned char* candidate = NULL;
	for (size_t i = 0; i < tx->nOutput; ++i) {
		LOG("\tOutput-" << i << " : " << EncodeBase58(outputs[i].addr, outputs[i].addr+ADDRBYTES)
						<< " value: " << outputs[i].value << endl);


		tmpaddr.assign(outputs[i].addr, outputs[i].addr+ADDRBYTES);
		auto it = addr2info.find(tmpaddr);
		if (it == addr2info.end()) {
			cerr << "updateClusters:  address not in addr2info: "
					<< EncodeBase58(outputs[i].addr, outputs[i].addr+ADDRBYTES) << endl;
			continue;
		}



		/*
		 * There is exactly one pk that appears only once up to now - this is H2 basic
		 *
		 * There is exactly one pk that appears only once in all transactions - this is H2 refined C
		 */
		if (blocked == false && it->second.curCount == 0) {	// output never occurred up to now
			if (candidate == NULL) {
				candidate = outputs[i].addr;
			} else {
				// two outputs that only occur once --> abort
				blocked = true;
			}
		}

		if (!blocked && h2refinedC && candidate == outputs[i].addr && it->second.totalCount != 1) {
			blocked = true;
		}


		/*
		 * There is no public key within the outputs which also appears on the input side (self-change address)
		 * Do this
		 */
		for (size_t i = 0; i < tx->nInput && (h2refinedB || !blocked); ++i) {
			output_s* refoutput = (output_s*)(bufbegin + offsets[i]);

			if (memcmp(outputs[i].addr, refoutput->addr, ADDRBYTES) == 0) {
				// output equals input --> abort
				blocked = true;

				it->second.selfchanged = true;
			}
		}




		/*
		 * H2 refined: remove change address if:
		 * there is an output that had already received exactly one input
		 */

		if (h2refinedA) {
			if (blocked == false && it->second.curCount == 1) {
				blocked = true;
			}
		}

		/*
		 * H2 refined: remove change address if:
		 * there is an output that had been used in a self-change transaction
		 */

		if (h2refinedB) {
			if (blocked == false && it->second.selfchanged == true) {
				blocked = true;
			}
		}

		++it->second.curCount;


	}


	if (blocked == false && candidate != NULL) {
		//cout << "h2 success" << endl;
		curAddrList.push_back(candidate);
	}

	return !blocked;
}

/*
 * value based heuristic
 */
bool HValue(addrlist_t& curAddrList, tx_s* tx, output_s outputs[], size_t offsets[]) {
	// find smallest input
	int64_t smallest = INT64_MAX;
	if (tx->nInput < 1 || tx->nOutput < 2) {
		return false;
	}

	for (size_t i = 0; i < tx->nInput; ++i) {

		output_s* refoutput = (output_s*)(bufbegin + offsets[i]);

		if (refoutput->value < smallest) {
			smallest = refoutput->value;
		}
	}


	unsigned char* candidate = NULL;
	for (size_t i = 0; i < tx->nOutput; ++i) {
		if (outputs[i].value < smallest) {
			if (candidate == NULL) {
				candidate = outputs[i].addr;
			} else {
				return false;
			}
		}
	}
	if (candidate != NULL) {
		curAddrList.push_back(candidate);
		return true;
	} else {
		return false;
	}
}


/*
 * growth based heuristic. Clears curAddrList if the largest cluster grows by
 * more than maxGrowth addresses by this transaction
 */
bool HGrowth(addrlist_t& curAddrList, tx_s* tx, output_s outputs[], size_t offsets[], size_t maxGrowth = 10) {
	static addr_t tmpaddr;
	size_t totalAddresses = 0;
	size_t largestClusterSize = 0;

	for (const auto addr : curAddrList) {
		tmpaddr.assign(addr, addr+ADDRBYTES);
		auto it = addr2info.find(tmpaddr);
		if (it != addr2info.end()) {
			size_t cursize = 1;
			if (it->second.cluster != NULL) {
				cursize = it->second.cluster->addresses.size();
			}
			totalAddresses += cursize;
			if (cursize > largestClusterSize) {
				largestClusterSize = cursize;
			}
		}
	}

	size_t diff = totalAddresses - largestClusterSize;

	if (diff > maxGrowth) {
		curAddrList.clear();
		return false;
	}

	return true;
}

void logCurTx(char* curptr) {
	//printf("txptr = %p\n", tx);
	tx_s* tx = (tx_s*)curptr;
	LOG("TX Hash: " << Hash2String(tx->hash) << " BlockID: " << tx->blockid
			<< " nInput: " << tx->nInput << " nOutput: " << tx->nOutput << endl);

	output_s* outputs = (output_s*) (curptr + sizeof(tx_s));
	for (size_t i = 0; i < tx->nOutput; ++i) {
		LOG("\tOutput-" << i << " : " << EncodeBase58(outputs[i].addr, outputs[i].addr+ADDRBYTES)
						<< " value: " << outputs[i].value << endl);
	}


	size_t* offsets = (size_t*) (curptr + sizeof(tx_s) + tx->nOutput * sizeof(output_s));
	for (size_t i = 0; i < tx->nInput; ++i) {
		output_s* refoutput = (output_s*)(bufbegin + offsets[i]);
		tx_s* reftx = (tx_s*)(bufbegin+refoutput->txoffs);

		LOG("\tInput from: " << EncodeBase58(refoutput->addr, refoutput->addr+ADDRBYTES)
				<< " TX = "<< Hash2String(reftx->hash) << endl);

	}
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		cout << "Partitions the set of addresses into clusters" << endl;
		cout << "USAGE: ./clusterize <txgraph> <txperaddr> options..." << endl;
		cout << "OPTIONS: h2, hvalue, hgrowth, printonly" << endl;
		cout << "\thvalue and hgrowth can be set to 1 or 0\n\thgrowth takes the parameter k" << endl;
		cout << "\th2 is a bitmask to select the variants (1 -> H2, 2 -> H2a, 4 -> H2b, 8 -> H2c), variants can be combined" << endl << endl;

		cout << "EXAMPLE: ./clusterize txgraph.bin txperaddr.bin hvalue 1 hgrowth 20 h2 3" << endl;
		cout << "clusters are written to clusters.out.xxx.raw with xxx indicating the selected heuristics" << endl;
		return 1;
	}

	struct {
		int h2 = 0;
		bool hValue = false;
		int hGrowth = 0;
		bool printOnly = false;

		bool h2refinedA = false;
		bool h2refinedB = false;
		bool h2refinedC = false;
	} conf;

	vector<char> graph;
	int txcount = 0;

	for (int i = 3; i < argc; i+=2) {
		if (strcmp(argv[i], "h2") == 0) {
			conf.h2 = atoi(argv[i+1]);

			conf.h2refinedA = (conf.h2 & 2);
			conf.h2refinedB = (conf.h2 & 4);
			conf.h2refinedC = (conf.h2 & 8);
		}
		if (strcmp(argv[i], "hvalue") == 0) {
			conf.hValue = (*argv[i+1] == '1');
		}
		if (strcmp(argv[i], "hgrowth") == 0) {
			conf.hGrowth = atoi(argv[i+1]);
		}
		if (strcmp(argv[i], "printonly") == 0) {
			conf.printOnly = (*argv[i+1] == '1');
		}


	}

	cout << "h2: " << conf.h2 << " h2refinedA: " << conf.h2refinedA << " h2refinedB: " << conf.h2refinedB
			<< " h2refinedC: " << conf.h2refinedC <<" hValue: " << conf.hValue << " hGrowth: " << conf.hGrowth << endl;


	readTxCount(argv[2]);

	readFileToBuf(argv[1], graph);

	char* end = (&graph.back()+1);
	char* curptr = &graph[0];
	bufbegin = &graph[0];

	cout << "binout size: " << graph.size() << endl;



	addrlist_t curAddrList;		// pointer to the addresses (in and out) of the current TX that are in one cluster


	chrono::high_resolution_clock::time_point tLast;
	while (curptr < end) {
		if (++txcount % 100000 == 0) {
			chrono::high_resolution_clock::time_point tNow = chrono::high_resolution_clock::now();
		    auto duration = chrono::duration_cast<chrono::milliseconds>(tNow-tLast).count();
			cout << "txcount=" << txcount << " speed: " << (100000*1000)/duration
					<< " " << (curptr-bufbegin)*100.0 / (double)graph.size() << " %" 
					<<  "\tmemory: " << memory_used () << endl;
			tLast = tNow;
		}

		tx_s* tx = (tx_s*)curptr;
		output_s* outputs = (output_s*) (curptr + sizeof(tx_s));
		size_t* offsets = (size_t*) (curptr + sizeof(tx_s) + tx->nOutput * sizeof(output_s));

		curAddrList.clear();

#ifdef LOG_DEBUG_ENABLED
		logCurTx(curptr);
#endif

		if (!conf.printOnly) {
			H1(curAddrList, tx, outputs, offsets);
		}

		bool changeAddressAdded = false;
		if (conf.h2 != 0) {
			changeAddressAdded = H2(curAddrList, tx, outputs, offsets, conf.h2refinedA, conf.h2refinedB, conf.h2refinedC);
		}

		if (conf.hValue && !changeAddressAdded) {
			HValue(curAddrList, tx, outputs, offsets);
		}

		if (conf.hGrowth > 0) {
			HGrowth(curAddrList, tx, outputs, offsets, conf.hGrowth);
		}



		if (!conf.printOnly) {
			updateClusters(tx, curAddrList);
		}

		curptr += sizeof(tx_s) + tx->nInput * sizeof(size_t) + tx->nOutput * sizeof(output_s);
	}


	// add addresses without cluster as single-address cluster
	for (auto& ai : addr2info) {
		if (ai.second.cluster == NULL) {
			clusterlist.push_back({});
			ai.second.cluster = &clusterlist.back();
			unsigned char* tmpaddr = (unsigned char*)&ai.first[0];
			ai.second.cluster->addresses.push_back(tmpaddr);	// TODO this is dangerous as the hashmap might be relocated and the pointer invalidated
		}
	}

	for (auto it = clusterlist.begin(); it != clusterlist.end();/* increment below*/) {
		if (it->addresses.size() == 0) {
			clusterlist.erase(it++);
		} else {
			++it;
		}
	}

	size_t nclusters = clusterlist.size();
	cout << "####" << clusterlist.size() << endl;
	stringstream ss;

	ss << "clusters.out.h2" << conf.h2 << ".hValue" << conf.hValue << ".hGrowth" << conf.hGrowth << ".raw";
	ofstream clfile(ss.str(), std::ios::binary);
	clfile.write((char*)&nclusters, sizeof(size_t));

	for (auto& c : clusterlist) {
		if (--nclusters % 1000000 == 0) {
			cout << "output remaining clusters=" << nclusters << endl;
		}
		size_t csize = c.addresses.size();
		clfile.write((char*)&csize, sizeof(size_t));
		for (const unsigned char* a : c.addresses) {
			clfile.write((char*)a, ADDRBYTES);
		}


		csize = c.txes.size();
		clfile.write((char*)&csize, sizeof(size_t));
		for (const tx_s* tx : c.txes) {
			clfile.write((char*)tx->hash, HASHBYTES);
			clfile.write((char*)&tx->blockid, sizeof(int32_t));
		}
	}
	clfile.close();




}

