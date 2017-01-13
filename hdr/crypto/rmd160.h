#pragma once

#include <stddef.h>
#include <inttypes.h>
#include "ripemd160_btc.h"

enum { kRIPEMD160ByteSize = 20 };

void rmd160wr(
          uint8_t *result,
    const uint8_t *data,
           size_t len
) {
    CRIPEMD160 ctx;
    ctx.Write(data, len);
    ctx.Finalize(result);
}

