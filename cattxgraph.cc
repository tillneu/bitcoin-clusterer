#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <list>
#include <chrono>
#include <fstream>
#include <sstream>


#include "hdr/data.h"


using namespace std;

char* 				bufbegin;	// begin of tx graph buffer





void logCurTx(char* curptr) {
	//printf("txptr = %p\n", tx);
	tx_s* tx = (tx_s*)curptr;
	cout << "TX Hash: " << Hash2String(tx->hash) << " BlockID: " << tx->blockid
			<< " nInput: " << tx->nInput << " nOutput: " << tx->nOutput << endl;

	output_s* outputs = (output_s*) (curptr + sizeof(tx_s));
	for (size_t i = 0; i < tx->nOutput; ++i) {
		cout << "\tOutput-" << i << " : " << EncodeBase58(outputs[i].addr, outputs[i].addr+ADDRBYTES)
						<< " value: " << outputs[i].value << endl;
	}


	size_t* offsets = (size_t*) (curptr + sizeof(tx_s) + tx->nOutput * sizeof(output_s));
	for (size_t i = 0; i < tx->nInput; ++i) {
		output_s* refoutput = (output_s*)(bufbegin + offsets[i]);
		tx_s* reftx = (tx_s*)(bufbegin+refoutput->txoffs);

		cout << "\tInput from: " << EncodeBase58(refoutput->addr, refoutput->addr+ADDRBYTES)
				<< " TX = "<< Hash2String(reftx->hash) << endl;

	}
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Prints a binary blockchain file created by ./parsebc" << endl;
		cout << "<output> contains for all address the number of appearances as outputs of transactions." << endl << endl;
		cout << "USAGE: ./catgraph <input> [firstnbytesonly]" << endl;
		cout << "EXAMPLE: ./catgraph txgraph.bin 10000" << endl;
		cout << "firstnbytesonly makes the program read only the first bytes of <input>" << endl;
		return 1;
	}

	vector<char> graph;

	long maxBuf = 0;
	if (argc > 2) {
		maxBuf = atol(argv[2]);
	}

	readFileToBuf(argv[1], graph, maxBuf);

	char* end = (&graph.back()+1);
	char* curptr = &graph[0];
	bufbegin = &graph[0];


	addr_t tmpaddr;

	while (curptr < end) {
		tx_s* tx = (tx_s*)curptr;
		logCurTx (curptr);
		curptr += sizeof(tx_s) + tx->nInput * sizeof(size_t) + tx->nOutput * sizeof(output_s);
	}

}

