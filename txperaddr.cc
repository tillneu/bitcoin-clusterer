#include <iostream>
#include <fstream>
#include <sstream>

#include "hdr/data.h"

using namespace std;

char* 				bufbegin;	// begin of tx graph buffer


typedef vector<unsigned char> addr_t;
typedef unordered_map<addr_t, uint32_t, addr_hash<addr_t> > addr2count_t;


int main(int argc, char* argv[]) {
	if (argc < 3) {
		cout << "Parses a binary blockchain file created by ./parsebc and creates a new file <output>." << endl;
		cout << "<output> contains for all address the number of appearances as outputs of transactions." << endl << endl;
		cout << "USAGE: ./txperaddr <input> <output> [firstnbytesonly]" << endl;
		cout << "USAGE: ./txperaddr txgraph.bin txperaddr.bin" << endl;
		cout << "<output> will be overwritten, firstnbytesonly makes the program read only the first bytes of <input>" << endl;
		return 1;
	}


	vector<char> graph;
	vector<char> outid2offset;
	int txcount = 0;

	long maxBuf = 0;
	if (argc > 3) {
		maxBuf = atol(argv[3]);
	}

	readFileToBuf(argv[1], graph, maxBuf);

	char* end = (&graph.back()+1);
	char* curptr = &graph[0];


	addr2count_t addr2count;
	addr_t tmpaddr;

	while (curptr < end) {
		if (++txcount % 1000000 == 0) {
			cout << "txcount=" << txcount << " addr2count.size: " << addr2count.size() << endl;
		}

		tx_s* tx = (tx_s*)curptr;
		output_s* outputs = (output_s*) (curptr + sizeof(tx_s));
		//size_t* offsets = (size_t*) (curptr + sizeof(tx_s) + tx->nOutput * sizeof(output_s));

		for (size_t i = 0; i < tx->nOutput; ++i) {
			LOG("\tOutput-" << i << " : " << EncodeBase58(outputs[i].addr, outputs[i].addr+ADDRBYTES)
								<< " value: " << outputs[i].value << endl);

			tmpaddr.assign(outputs[i].addr, outputs[i].addr+ADDRBYTES);

			auto it = addr2count.find(tmpaddr);
			if (it == addr2count.end()) {
				addr2count.emplace(tmpaddr, 1);
			} else {
				it->second++;
			}

		}


		curptr += sizeof(tx_s) + tx->nInput * sizeof(size_t) + tx->nOutput * sizeof(output_s);

	}



	ofstream txout(argv[2], std::ios::binary);
	size_t addrcount = addr2count.size();
	for (auto& v : addr2count) {
		if (--addrcount % 1000000 == 0) {
			cout << "output remaining addrcountcount=" << addrcount << " addr2count.size: " << addr2count.size() << endl;
		}
		txout.write((char*)&v.first[0], ADDRBYTES);
		txout.write((char*)&v.second, sizeof(uint32_t));
	}
	txout.close();

}

