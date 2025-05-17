#pragma once

//#define DATA_TO_SEND 1000000000 // 1gb
//#define DATA_TO_SEND 100000000 // 100mb
//#define DATA_TO_SEND 10000000 // 10mb
//#define DATA_TO_SEND 6000000 // 6mb
#define DATA_TO_SEND 1000000 // 1mb
//#define DATA_TO_SEND 100000 // 100kb


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
