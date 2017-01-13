/*
 * based on code from bitiodine
 */



#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hdr/script.h"
#include "hdr/data.h"
#include "hdr/base58.h"
#include "hdr/blockchain.h"

using namespace std;



unordered_map<hash_t,Block*, container_hash<hash_t> > gBlockMap;

// utxo: for each tx hash the number of unspent outputs and the offset of the tx in the output file
unordered_map<hash_t, pair<uint32_t, size_t>, hash_hash<hash_t>> utxo;
uint64_t gChainSize;
Block* genesisBlock = NULL;
Block* newestBlock = NULL;
ofstream graphout;
hash_t tmphash(HASHBYTES);





vector<BlockFile> findBlockFiles(const string& blockChainDir) {
	int blkDatId = 0;
	const char* fmt = "/blk%05d.dat";

	vector<BlockFile> blockFiles;

	while(1) {

		char buf[64];
		sprintf(buf, fmt, blkDatId++);

		auto fileName = blockChainDir + std::string(buf) ;
		auto fd = open(fileName.c_str(), O_RDONLY);
		if(fd < 0) {
			if(1 < blkDatId) {
				break;
			}
			cerr << "failed to open block chain file" << fileName << endl;
		}

		struct stat statBuf;
		auto r0 = fstat(fd, &statBuf);
		if(r0<0) {
			cerr << "failed to fstat block chain file" << fileName << endl;
		}

		auto fileSize = statBuf.st_size;

		BlockFile blockFile;
		blockFile.fd = fd;
		blockFile.size = fileSize;
		blockFile.name = fileName;
		blockFile.read = false;
		blockFiles.push_back(blockFile);
		gChainSize += fileSize;
	}
	cout << "block chain size = " << 1e-9*gChainSize << " gb" << endl;

	return blockFiles;
}

bool getBlockHeader(size_t& size, Block *prev, hash_t& hash, hash_t& prevBlockHash,
		size_t& earlyMissCnt, const uint8_t *p) {

	LOAD(uint32_t, magic, p);
	if(gExpectedMagic != magic) {
		return false;
	}

	LOAD(uint32_t, sz, p);
	size = sz;
	prev = 0;


	sha256Twice(&hash[0], p, gHeaderSize);

	prevBlockHash.assign(p + 4, p+4+HASHBYTES);
	auto it = gBlockMap.find(prevBlockHash);
	if(it != gBlockMap.end()) {
		prev = it->second;
	} else {
		++earlyMissCnt;
	}
	return true;
}


void buildBlockHeaders(vector<BlockFile>& blockFiles, size_t maxBlock) {

	size_t nbBlocks = 0;
	size_t baseOffset = 0;
	size_t earlyMissCnt = 0;
	uint8_t buf[8+gHeaderSize];
	const auto sz = sizeof(buf);

	cout << "reading block headers ..." << endl;
	for(auto &blockFile : blockFiles) {
		if (blockFile.read == true) {
			continue;
		}

		while(1) {

			auto nbRead = read(blockFile.fd, buf, sz);
			if(nbRead<(signed)sz) {
				break;
			}


			hash_t hash(HASHBYTES);
			hash_t prevBlockHash(HASHBYTES);
			Block *prevBlock = NULL;
			size_t blockSize = 0;

			bool rc = getBlockHeader(blockSize, prevBlock, hash, prevBlockHash, earlyMissCnt,buf);
			if (rc == 0) {
				break;
			}

			auto where = lseek(blockFile.fd, (blockSize + 8) - sz, SEEK_CUR);
			auto blockOffset = where - blockSize;
			if(where<0) {
				break;
			}


			auto block = new Block(hash, &blockFile, blockSize, prevBlock, prevBlockHash, blockOffset);
			if (genesisBlock == NULL) {
				genesisBlock = block;
				block->height = 0;
			}

			if (nbBlocks <= maxBlock) {
				newestBlock = block;
			}

			//block->print();
			gBlockMap[hash] = block;
			if (++nbBlocks % 10000 == 0) {
				cout << "nBlocks " << nbBlocks << endl;
			}


		}
		blockFile.read = true;
		baseOffset += blockFile.size;

		if (nbBlocks > maxBlock) {
			cout << "number of blocks: " << gBlockMap.size() << endl;
			return;
		}

	}

	if (nbBlocks == 0) {
		cerr << "found no blocks - giving up" << endl;
		exit(1);
	}


	cout << "number of blocks: " << gBlockMap.size() << endl;
}


bool computeBlockHeights() {
	cout << "link all blocks ..." << endl;

	Block* b = newestBlock;
	while (b != genesisBlock) {
		//b->print();
		if (b->prev == NULL) {
			auto it = gBlockMap.find(b->prevHash);
			if (it == gBlockMap.end()) {
				cout << "Blocks out of order, reading additional file" << endl;
				return false;
			}
			b->prev = it->second;
		}

		b->prev->next = b;
		b = b->prev;
	}

	// postcondition --> b == genesisBlock

	b->height = 0;
	b = b->next;
	int64_t maxHeight = 0;
	while (b != NULL) {
		b->height = b->prev->height + 1;
		//b->print();
		maxHeight = b->height;
		b = b->next;
	}

	cout << "length of longest chain: " << maxHeight << endl;
	return true;
}

uint32_t findTxEndAndNOutput(const uint8_t*& p) {
	SKIP(uint32_t, nVersion, p);
	LOAD_VARINT(nbInputs, p);
	for (uint64_t inputIndex = 0; inputIndex < nbInputs; ++inputIndex) {


		SKIP(uint256_t, upTXhash, p);
		SKIP(uint32_t, upOutputIndex, p);

		LOAD_VARINT(inputScriptSize, p);
		p += inputScriptSize;
		SKIP(uint32_t, sequence, p);
	}
	LOAD_VARINT(nbOutputs, p);
	for (uint64_t outputIndex = 0; outputIndex < nbOutputs; ++outputIndex) {
		SKIP(uint64_t, value, p);
		LOAD_VARINT(outputScriptSize, p);
		p += outputScriptSize;
	}

	SKIP(uint32_t, lockTime, p);
	return nbOutputs;
}

void parseTX(const Block* block, const uint8_t*& p) {
	static tx_s tx;

	auto txend = p;
	tx.nOutput = findTxEndAndNOutput(txend);
	tx.blockid = block->height;
	sha256Twice(tx.hash, p, txend - p);

	SKIP(uint32_t, nVersion, p);
	LOG("\tTX: " << Hash2String(tx.hash) << endl);

	/*
	 * inputs
	 */
	LOAD_VARINT(nbInputs, p);	// dont use this directly as it also includes coinbase inputs


	output_s output;	// reused for all outputs
	output.txoffs = graphout.tellp();



	LOG("\t\tInputs (" << nbInputs << "): " << endl);

	vector<size_t> inputoffsets;
	for (uint64_t inputIndex = 0; inputIndex < nbInputs; ++inputIndex) {
		if (memcmp(nullHash, p, HASHBYTES) != 0) {
			tmphash.assign(p, p + HASHBYTES);
			p += HASHBYTES;
			LOAD(uint32_t, upOutputIndex, p);

			auto it = utxo.find(tmphash);
			if (it == utxo.end()) {
				cerr << "Hash not found: " << Hash2String(tmphash) << endl;
				exit(1);
			}
			inputoffsets.push_back(it->second.second + sizeof(tx_s) + upOutputIndex * sizeof(output_s));

			if (--it->second.first == 0) {
				utxo.erase(it);
			}

			LOG("\t\t\tClaiming TX: " << Hash2String(tmphash) << "\tindex: " << upOutputIndex << endl);
		} else {
			// coinbase input
			LOG("\t\t\tCoinbase Input" << endl);
			p += HASHBYTES + sizeof(uint32_t);
		}

        LOAD_VARINT(inputScriptSize, p);
        p += inputScriptSize;
        SKIP(uint32_t, sequence, p);
	}


	tx.nInput = inputoffsets.size();
	graphout.write((char*)&tx, sizeof(tx));


	/*
	 * outputs
	 */
	LOAD_VARINT(nbOutputs, p);
	LOG("\t\tOutputs (" << nbOutputs << "): " << endl);
	for (uint64_t outputIndex = 0; outputIndex < nbOutputs; ++outputIndex) {
        LOAD(uint64_t, value, p);
        LOAD_VARINT(outputScriptSize, p);

        uint8_t   addrType[3];
        uint160_t pubKeyHash;
        solveOutputScript(pubKeyHash.v, p, outputScriptSize, addrType);



        hash160ToAddr(output.addr, pubKeyHash.v);

        output.value = value;
        graphout.write((char*)&output, sizeof(output));

        LOG("\t\t\t" << EncodeBase58(output.addr, output.addr+ADDRBYTES) << "\tValue: " << value << "\tType: " << type << endl);

        p += outputScriptSize;
	}

	// write inputs to file
	graphout.write((char*)&inputoffsets[0], sizeof(size_t) * inputoffsets.size());

	// add tx hash to utxo set
	tmphash.assign(tx.hash, tx.hash+HASHBYTES);
	utxo.emplace(piecewise_construct, forward_as_tuple(tmphash), forward_as_tuple(tx.nOutput, output.txoffs));

	SKIP(uint32_t, lockTime, p);

}

void parseLongestChain(int maxBlock) {
	cout << "parseLongestChain ..." << endl;

	Block* b = genesisBlock;

	while (b != NULL && b->height <= maxBlock) {
		auto p = b->chunk->getData();

        SKIP(uint32_t, version, p);
        SKIP(uint256_t, prevBlkHash, p);
        SKIP(uint256_t, blkMerkleRoot, p);
        SKIP(uint32_t, blkTime, p);
        SKIP(uint32_t, blkBits, p);
        SKIP(uint32_t, blkNonce, p);

        LOAD_VARINT(nbTX, p);
        if (b->height % 1000 == 0) {
        	cout << "Height: " << b->height << " " << Hash2String(b->hash) << " nTX: " << nbTX << "\tUTXO: " << utxo.size() << endl;
        }
        for(uint64_t txIndex = 0; txIndex < nbTX; ++txIndex) {
			parseTX(b, p);	// parseTX moves p forward through the block
		}

		b->chunk->releaseData();

		b = b->next;
	}


}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		cout << "Parses all bitcoin blocks in a directory and creates a new file <output>." << endl;
		cout << "<output> is a pointer-based binary file of the transaction graph" << endl << endl;
		cout << "USAGE: ./parsebc <blockchaindir> <output> [maxBlock]" << endl;
		cout << "EXAMPLE: ./parsebc /home/till/.bitcoin/blocks txgraph.bin" << endl;
		cout << "<output> will be overwritten" << endl;
		return 1;
	}

	int maxBlock = INT_MAX;
	if (argc > 3) {
		maxBlock = atoi(argv[3]);
		cout << "parsing up to block " << maxBlock << endl;
	}

	auto files = findBlockFiles(argv[1]);

	bool rc;
	do {
		buildBlockHeaders(files, maxBlock);
		rc = computeBlockHeights();
	} while (rc == false);

	cout << "writing to " << argv[2] << endl;
	graphout.open(argv[2], std::ifstream::binary);



	parseLongestChain(maxBlock);
	graphout.close();
}
