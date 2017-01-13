#pragma once

#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>

#include "base58.h"
#include "maputils.h"

#define ADDRBYTES 25
#define HASHBYTES 32
#define IPBYTES   16

/*
 * the binary graph file contains the structures ts_s and output_s, and pointers to inputs.
 *
 * For each transaction there is ONE tx_s
 * Then, there are nOutput x output_s.
 *   txoffs points to the transaction (as an offset in the file)
 * Then, there are nInput pointers to output_s (of previous transactions) referencing the inputs
 *
 */
struct tx_s {
    unsigned char hash[HASHBYTES];
    int32_t blockid;
    uint32_t nOutput;
    uint32_t nInput;
};
// nOutput output_s that represent the outputs of the transaction
struct output_s {
    unsigned char addr[ADDRBYTES];
    int64_t value;
    size_t txoffs;	// offset pointer to transaction
};
// nInput pointers to output_s that represent the inputs of the transaction




struct db_s {
    unsigned char hash[HASHBYTES];
    int32_t blockid;
    vector<pair<output_s, uint32_t>> outputs;
    vector<size_t> inputs;	// Id of singleoutputs;
};


typedef vector<unsigned char*> addrlist_t;
struct cluster_s {
	addrlist_t addresses;
	vector<tx_s*> txes;
};

typedef vector<unsigned char> addr_t;
struct addrinfo_s {
	cluster_s* 	cluster;
	uint32_t 	totalCount;
	uint32_t	curCount;
	bool 		selfchanged;


	addrinfo_s (cluster_s* c, uint32_t tc, uint32_t cc, bool sc)
	: cluster(c), totalCount(tc), curCount(cc), selfchanged(sc) { };
};

typedef unordered_map<addr_t, addrinfo_s, addr_hash<addr_t> > addr2info_t;

//#define LOG_DEBUG_ENABLED

#ifdef LOG_DEBUG_ENABLED

#define LOG(msg) \
  do {                      \
    cout << msg;    \
  } while (false)

#else

#define LOG(msg) \
  do {                      \
  } while (false)
#endif


size_t memory_used ()
{
	// Ugh, getrusage doesn't work well on Linux.  Try grabbing info
	// directly from the /proc pseudo-filesystem.  Reading from
	// /proc/self/statm gives info on your own process, as one line of
	// numbers that are: virtual mem program size, resident set size,
	// shared pages, text/code, data/stack, library, dirty pages.  The
	// mem sizes should all be multiplied by the page size.
	size_t size = 0;
	FILE *file = fopen("/proc/self/statm", "r");
	if (file) {
		unsigned int vm = 0;
		int rc = fscanf (file, "%ul", &vm);  // Just need the first num: vm size
		if (rc < 0) {
			return 0;
		}
		fclose (file);
	   size = (size_t)vm * getpagesize();
	}
	return size;
}


bool readFileToBuf(const string fname, vector<char>& buf, const streamsize maxBuf = 0) {
	std::ifstream file(fname, std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	if (maxBuf != 0 && maxBuf < size) {
		size = maxBuf;
	}

	file.seekg(0, std::ios::beg);

	cout << "allocating " << size << endl;
	buf.resize(size);
	if (file.read(buf.data(), size)) {
	    return true;
	}
	return false;
}




/*
 * hash handling
 */

typedef vector<uint8_t> hash_t;

string Hash2String(const hash_t& hash)
{
	size_t size = hash.size();
    char psz[hash.size() * 2 + 1];
    for (unsigned int i = 0; i < size; i++)
        sprintf(psz + i * 2, "%02x", hash[size - i - 1]);
    return std::string(psz, psz + size * 2);
}


hash_t String2Hash(const char* psz) {
	// hex string to uint
	const size_t width = 256/8;
	hash_t data(width);

	const char* pbegin = psz;
	while (::HexDigit(*psz) != -1) {
		psz++;
	}
	psz--;
	unsigned char* p1 = (unsigned char*)&data[0];
	unsigned char* pend = p1 + width;
	while (psz >= pbegin && p1 < pend) {
		*p1 = ::HexDigit(*psz--);
		if (psz >= pbegin) {
			*p1 |= ((unsigned char)::HexDigit(*psz--) << 4);
			p1++;
		}
	}

	return data;
}

hash_t String2Hash(const string psz) {
	return String2Hash(psz.c_str());
}


/*
 * end hash handling
 */
