#pragma once

#include <stddef.h>
#include <inttypes.h>
#include "sha256_btc.h"


enum { kSHA256ByteSize = 32 };


void sha256wr(
    uint8_t       *result,
    const uint8_t *data,
    size_t        len
) {
    CSHA256 ctx;
    ctx.Write(data, len);
    ctx.Finalize(result);
}

