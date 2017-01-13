/*
 * based on code from bitiodine
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include "blockchain.h"
#include "crypto/sha256.h"
#include "crypto/rmd160.h"


static inline void sha256Twice(uint8_t *sha, const uint8_t *buf, uint64_t size) {
	sha256wr(sha, buf, size);
	sha256wr(sha, sha, kSHA256ByteSize);
}


static bool getOpPushData(
    const uint8_t *&p,
    uint64_t &dataSize
) {

    dataSize = 0;
    LOAD(uint8_t, c, p);

    bool isImmediate = (0<c && c<79);
    if(!isImmediate) {
        --p;
        return false;
    }

         if((c<=75)) {                       dataSize = c; }
    else if((76==c)) { LOAD( uint8_t, v, p); dataSize = v; }
    else if((77==c)) { LOAD(uint16_t, v, p); dataSize = v; }
    else if((78==c)) { LOAD(uint32_t, v, p); dataSize = v; }
    if(512*1024<dataSize) {
        return false;
    }

    p += dataSize;
    return true;
}

bool isMultiSig(
    int &_m,
    int &_n,
    std::vector<uint160_t> &addresses,
    const uint8_t *p,
    size_t scriptSize
) {

    auto e = scriptSize + p;
    if(scriptSize<=5) {
        return false;
    }

    auto m = (*(p++) - 0x50);             // OP_1 ... OP-16
    auto isMValid = (1<=m && m<=16);
    if(!isMValid) {
        return false;
    }

    int count = 0;
    while(1) {
        uint64_t dataSize = 0;
        auto ok = getOpPushData(p, dataSize);
        if(e<=p) {
            return false;
        }
        if(!ok) {
            break;
        }

        uint160_t addr;
        auto sz = sizeof(addr);
        memcpy(addr.v, p-sz, sz);
        addresses.push_back(addr);
        ++count;
    }

    auto n = (*(p++) - 0x50);             // OP_1 ... OP-16
    auto isNValid = (1<=n && n<=16);
    if(!isNValid || n!=count) {
        return false;
    }

    auto lastOp = *(p++);
    bool ok = (0xAE==lastOp) &&          // OP_CHECKMULTISIG
              (m<=n)         &&
              (p==e);
    if(ok) {
        _m = m;
        _n = n;
    }
    return ok;
}

bool isCommentScript(
    const uint8_t *p,
    size_t scriptSize
) {
    const uint8_t *e = scriptSize + p;
    while((p<e)) {
        LOAD(uint8_t, c, p);
        bool isImmediate = (0<c && c<79);
        if(!isImmediate) {
            if(0x6A!=c) {
                return false;
            }
        } else {

            --p;

            uint64_t dataSize = 0;
            auto ok = getOpPushData(p, dataSize);
            if(!ok) {
                return false;
            }
        }
    }
    return true;
}

struct Compare160 {
    bool operator()(
        const uint160_t &a,
        const uint160_t &b
    ) const {
        auto as = a.v;
        auto bs = b.v;
        auto ae = kRIPEMD160ByteSize + as;
        while(as<ae) {
            int delta = ((int)*(as++)) - ((int)*(bs++));
            if(delta) {
                return (delta<0);
            }
        }
        return true;
    }
};

static void packMultiSig(
                   uint8_t *pubKeyHash,
    std::vector<uint160_t> &addresses,
                       int m,
                       int n
) {
    std::sort(
        addresses.begin(),
        addresses.end(),
        Compare160()
    );

    std::vector<uint8_t> data;
    data.reserve(2 + kRIPEMD160ByteSize*sizeof(addresses));
    data.push_back((uint8_t)m);
    data.push_back((uint8_t)n);
    for(const auto &addr:addresses) {
        data.insert(
            data.end(),
            addr.v,
            kRIPEMD160ByteSize + addr.v
        );
    }
    rmd160wr(
        pubKeyHash,
        &(data[0]),
        data.size()
    );
}


int solveOutputScript(
          uint8_t *pubKeyHash,
    const uint8_t *script,
    uint64_t      scriptSize,
    uint8_t       *addrType
) {
    // default: if we fail to solve the script, we make it pay to unspendable hash 0 (lost coins)
    memset(pubKeyHash, 0, kRIPEMD160ByteSize);

    // The most common output script type that pays to hash160(pubKey)
    if(
            0x76==script[0]              &&  // OP_DUP
            0xA9==script[1]              &&  // OP_HASH160
              20==script[2]              &&  // OP_PUSHDATA(20)
            0x88==script[scriptSize-2]   &&  // OP_EQUALVERIFY
            0xAC==script[scriptSize-1]   &&  // OP_CHECKSIG
              25==scriptSize
    ) {
        memcpy(pubKeyHash, 3+script, kRIPEMD160ByteSize);
        addrType[0] = 0;
        return 0;
    }

    // Output script commonly found in block reward TX, that pays to an explicit pubKey
    if(
              65==script[0]             &&  // OP_PUSHDATA(65)
            0xAC==script[scriptSize-1]  &&  // OP_CHECKSIG
              67==scriptSize
    ) {
        uint256_t sha;
        sha256wr(sha.v, 1+script, 65);
        rmd160wr(pubKeyHash, sha.v, kSHA256ByteSize);
        addrType[0] = 0;
        return 1;
    }

    // A rather unusual output script that pays to and explicit compressed pubKey
    if(
              33==script[0]            &&  // OP_PUSHDATA(33)
            0xAC==script[scriptSize-1] &&  // OP_CHECKSIG
              35==scriptSize
    ) {
        //uint8_t pubKey[65];
        //bool ok = decompressPublicKey(pubKey, 1+script);
        //if(!ok) return -3;

        uint256_t sha;
        sha256wr(sha.v, 1+script, 33);
        rmd160wr(pubKeyHash, sha.v, kSHA256ByteSize);
        addrType[0] = 0;
        return 2;
    }

    // A modern output script type, that pays to hash160(script)
    if(
            0xA9==script[0]             &&  // OP_HASH160
              20==script[1]             &&  // OP_PUSHDATA(20)
            0x87==script[scriptSize-1]  &&  // OP_EQUAL
              23==scriptSize
    ) {
        memcpy(pubKeyHash, 2+script, kRIPEMD160ByteSize);
        addrType[0] = 5;
        return 3;
    }

    int m = 0;
    int n = 0;
    std::vector<uint160_t> addresses;
    if(
        isMultiSig(
            m,
            n,
            addresses,
            script,
            scriptSize
        )
    ) {
        packMultiSig(pubKeyHash, addresses, m, n);
        addrType[0] = 8;
        return 4;
    }

    // Broken output scripts that were created by p2pool for a while
    if(
        0x73==script[0] &&                  // OP_IFDUP
        0x63==script[1] &&                  // OP_IF
        0x72==script[2] &&                  // OP_2SWAP
        0x69==script[3] &&                  // OP_VERIFY
        0x70==script[4] &&                  // OP_2OVER
        0x74==script[5]                     // OP_DEPTH
    ) {
        return -2;
    }

    // A non-functional "comment" script
    if(isCommentScript(script, scriptSize)) {
        return -3;
    }

    // A challenge: anyone who can find X such that 0==RIPEMD160(X) stands to earn a bunch of coins
    if(
        0x76==script[0] &&                  // OP_DUP
        0xA9==script[1] &&                  // OP_HASH160
        0x00==script[2] &&                  // OP_0
        0x88==script[3] &&                  // OP_EQUALVERIFY
        0xAC==script[4]                     // OP_CHECKSIG
    ) {
        return -4;
    }

#if 0
    // TODO : some scripts are solved by satoshi's client and not by the above. track them
    // Unknown output script type -- very likely lost coins, but hit the satoshi script solver to make sure
    int result = extractAddress(pubKeyHash, script, scriptSize);
    if(result) return -1;
    return 5;
    printf("EXOTIC OUTPUT SCRIPT:\n");
    showScript(script, scriptSize);
#endif

    // Something we didn't understand
    return -1;
}



/*
 * addr must be 25 bytes long (ADDRBYTES)
 */
void hash160ToAddr(uint8_t addr[], const uint8_t pubKeyHash[]) {
	addr[0] = 0;
	memcpy(&addr[1], pubKeyHash, kRIPEMD160ByteSize);

	uint8_t tmphash[HASHBYTES];

	sha256Twice(tmphash, &addr[0], kRIPEMD160ByteSize + 1);

	memcpy(&addr[21], tmphash, 4);

}
