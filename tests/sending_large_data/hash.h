#pragma once

//#define DATA_TO_SEND 100000 // 100kb
#define DATA_TO_SEND 10000 // 1kb

#include <stdint.h>
#include <unistd.h>

unsigned long long quickhash64(uint8_t* data, size_t length)
{
    const unsigned long long mulp = 2654435789; 
    unsigned long long mix = 12565;

    mix ^= 104395301;

    for (size_t i = 0; i < length; ++i) {
        mix += (data[i] * mulp) ^ (mix >> 23);
    }

    return mix ^ (mix << 37);
}
