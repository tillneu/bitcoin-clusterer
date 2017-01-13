#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <math.h>

#include "hdr/data.h"

using namespace std;


string makeNiceNumber(double x, size_t maxN = 100000) {
	size_t n = (size_t) x;
	stringstream ss;

	if (n <= 100000) {
		string numWithCommas = to_string(n);
		int insertPosition = numWithCommas.length() - 3;
		while (insertPosition > 0) {
		    numWithCommas.insert(insertPosition, ",");
		    insertPosition-=3;
		}
		return numWithCommas;
	} else {
		ss << "$10^{" << (int) round(log(n)/log(10)) <<"}$";
	}

	return ss.str();
}

template<typename T>
vector<pair<int, pair<double, double>>> genLogHist(const vector<T>& values, double logbase, T max = 0) {
	vector<pair<int, pair<double, double>>> hist;

	if (max == 0) {
		for (const auto v : values) {
			if (v > max)
				max = v;
		}
	}

	size_t nbins = (size_t) (log(max)/log(logbase) + 1);
	hist.resize(nbins);
	for (const auto v : values) {
		size_t pos = (size_t) (log(v)/log(logbase));
		if (pos >= hist.size()) {
			cerr << "value too large " << v << " -> " << log(v)/log(logbase) << " -> " << pos << endl;
			continue;
		}
		hist[pos].first++;
	}

	for (size_t i = 0; i < nbins; ++i) {
		hist[i].second.first = pow(logbase, i); // this value is part of the bin [
		hist[i].second.second = pow(logbase, i+1); // this value is NOT part of the bin )
	}

	return hist;
}

void readClusters(const string& fname, const string& foutname, const int logbase) {

	vector<int> addrcounts;
	size_t maxsize = 0;
	size_t ccount;
	size_t addresses = 0;
	size_t transactions = 0;
	unsigned char addr[ADDRBYTES];
	unsigned char hash[HASHBYTES];

	int blockid;
	ifstream file(fname, ios::binary);
	ofstream fout(foutname);

	file.read((char*)&ccount, sizeof(size_t));
	fout << "# number of clusters: " << ccount << endl << endl;

	for (size_t i = 0; i < ccount; ++i) {
		if (i % 1000000 == 0) {
			cout << i << "\t" << i/(double)ccount*100 << "%" << endl;
		}
		size_t csize;
		file.read((char*)&csize, sizeof(size_t));
		addrcounts.push_back(csize);
		if (csize > maxsize) {
			maxsize = csize;
		}
		LOG(csize << " Addresses:" << endl);
		for (size_t j = 0; j < csize; ++j) {
			++addresses;
			file.read((char*)addr, ADDRBYTES);
			LOG("\t" << EncodeBase58(addr, addr+ADDRBYTES) << endl);
		}

		size_t tsize;
		file.read((char*)&tsize, sizeof(size_t));
		LOG(tsize << " Transactions:" << endl);
		for (size_t j = 0; j < tsize; ++j) {
			++transactions;
			file.read((char*)hash, HASHBYTES);
			file.read((char*)&blockid, sizeof(int32_t));
			LOG("\t\t" << Hash2String(hash) << " in block " << blockid << endl);
		}

	}

	file.close();

	size_t quantil = (addrcounts.size()*9)/10;
	nth_element(addrcounts.begin(), addrcounts.begin() + quantil, addrcounts.end());

	fout << "# total number addresses: " << addresses << endl;
	fout << "# total number transactions: " << transactions << endl << endl;

	fout << "# avg size of clusters: " << addresses/(double)ccount << endl;
	fout << "# 99% quantil size of clusters: " << addrcounts[quantil]  << endl << endl;
	fout << "# size of largest cluster: " << maxsize << endl;


	size_t ones = 0;
	for (const auto v : addrcounts) {
		if (v == 1)
			++ones;
	}
	fout << "# number of clusters with size one: " << ones << endl;

	fout << "# table: "<< fname << "\t" << ccount << " & " << addresses/(double)ccount << " & "
			<< addrcounts[quantil] << " & " <<  maxsize << " & " << ones << "\\\\" << endl;


	fout << endl;

	auto hist = genLogHist<int>(addrcounts, logbase, maxsize);

	fout << "# format: count\tlower bound included[\tupper bound excluded)" << endl;
	for (const auto& v : hist) {
		fout << v.first << "\t" << v.second.first << "\t" << v.second.second
				<< "\t$\\\\lbrack$" << makeNiceNumber(v.second.first) << "-" << makeNiceNumber(v.second.second) << ")" << endl;
	}


	fout.close();

}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Generates a histogram of the cluster sizes for a cluster file created by ./clusterize" << endl;
		cout << "USAGE: ./histcluster <clusters.out.xxx> <logbase>" << endl;
		cout << "EXAMPLE: ./histcluster clusters.out.h23.hValue1.hGrowth20.raw 10" << endl;
		cout << "the output is written to clusters.out.xxx.gpd" << endl;
		return 1;
	}
	stringstream ss;

	ss << argv[1] << ".gpd";
	readClusters(argv[1], ss.str(), atoi(argv[2]));

}
