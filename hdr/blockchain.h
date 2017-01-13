/*
 * based on code from bitiodine
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <string>
#include <iostream>

#include "data.h"

using namespace std;

#define SKIP(type, var, p)       \
		p += sizeof(type)            \

#define LOAD(type, var, p)       \
		type var = *(type*)p;        \
		p += sizeof(type)            \

#define LOAD_VARINT(var, p)      \
		uint64_t var = loadVarInt(p) \

static inline uint64_t loadVarInt(
		const uint8_t *&p
) {
	uint64_t r = *(p++);
	if((r<0xFD))  {                       return r; }
	if((0xFD==r)) { LOAD(uint16_t, v, p); return v; }
	if((0xFE==r)) { LOAD(uint32_t, v, p); return v; }
	LOAD(uint64_t, v, p); return v;
}

#define HASHBYTES 32

static const uint32_t gExpectedMagic = 0xd9b4bef9;
static const size_t gHeaderSize = 80;
struct uint160_t { uint8_t v[20]; };
struct uint256_t { uint8_t v[HASHBYTES]; };
static const char nullHash[HASHBYTES] = {};








struct BlockFile {
	int fd;
	uint64_t size;
	string name;
	bool read;
};

struct Chunk {

private:
	size_t size;
	size_t offset;
	mutable uint8_t *data;
	const BlockFile *blockFile;

public:
	void init(const BlockFile *_blockFile, size_t _size, size_t _offset) {
		data = 0;
		size = _size;
		offset = _offset;
		blockFile = _blockFile;
	}

	const uint8_t *getData() const {
		if (0 == data) {
			auto where = lseek64(blockFile->fd, offset, SEEK_SET);
			if(where!=(signed)offset) {
				cerr << "failed to seek into block chain file " << blockFile->name << endl;
			}
			data = (uint8_t*)malloc(size);

			auto sz = read(blockFile->fd, data, size);
			if(sz!=(signed)size) {
				//fatal("can't read block");
			}
		}
		return data;
	}

	void releaseData() const {
		free(data);
		data = 0;
	}

	size_t getSize() const                { return size;      }
	size_t getOffset() const              { return offset;    }
	const BlockFile *getBlockFile() const { return blockFile; }
};

struct Block {
	Chunk		*chunk;
	hash_t 		hash;
	int64_t		height;
	Block		*prev;
	Block		*next;
	hash_t		prevHash;

	Block (const hash_t& _hash, const BlockFile *_blockFile,
			size_t _size, Block *_prev,	hash_t& _prevHash, uint64_t _offset)

	: hash(_hash), height(-1), prev(_prev), next(NULL) {

		chunk = (Chunk*)malloc(sizeof(Chunk));
		chunk->init(_blockFile, _size, _offset);

		if (_prev == NULL) {
			prevHash.assign(_prevHash.begin(), _prevHash.end());
		}
	}

	void print() {
		cout << Hash2String(hash) << "\tHeight: " << height << "\tPrev: ";
		if (prev != NULL) {
			cout << Hash2String(prev->hash);
		} else {
			cout << "NA";
		}
		cout << endl;
	}
};
