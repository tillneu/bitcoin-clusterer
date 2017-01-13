#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>


#define LOG_DEBUG_ENABLED
#include "hdr/data.h"

using namespace std;



void catClusters(const string& fname) {

	vector<int> addrcounts;
	size_t maxsize = 0;
	size_t ccount;
	size_t addresses = 0;
	size_t transactions = 0;
	unsigned char addr[ADDRBYTES];
	unsigned char hash[HASHBYTES];

	int blockid;
	ifstream file(fname, ios::binary);
	if (!file) {
		cerr << "can't open file" << endl;
		return;
	}

	file.read((char*)&ccount, sizeof(size_t));

	for (size_t i = 0; i < ccount; ++i) {
		LOG("\n### Cluster " << i << " ###\n");
		size_t csize;
		file.read((char*)&csize, sizeof(size_t));
		addrcounts.push_back(csize);
		if (csize > maxsize) {
			maxsize = csize;
		}
		LOG(csize << " Addresses:\n");
		for (size_t j = 0; j < csize; ++j) {
			++addresses;
			file.read((char*)addr, ADDRBYTES);
			LOG("\t" << EncodeBase58(addr, addr+ADDRBYTES) << "\n");
		}

		size_t tsize;
		file.read((char*)&tsize, sizeof(size_t));
		LOG(tsize << " Transactions:" << endl);
		for (size_t j = 0; j < tsize; ++j) {
			++transactions;
			file.read((char*)hash, HASHBYTES);
			file.read((char*)&blockid, sizeof(int32_t));
			LOG("\t" << Hash2String(hash) << " in block " << blockid << "\n");
		}

	}

	file.close();

}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Outputs the addresses in the clusters for a cluster file created by ./clusterize" << endl;
		cout << "USAGE: ./catcluster <clusters.out.xxx.raw>" << endl;
		cout << "EXAMPLE: ./catcluster clusters.out.h23.hValue1.hGrowth20.raw" << endl;
		return 1;
	}

	catClusters(argv[1]);

}
