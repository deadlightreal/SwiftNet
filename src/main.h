#pragma once

#include <stdint.h>

typedef struct {
    uint16_t port;
} ClientInfo;

#ifndef RELEASE_MODE
    #define Debug(code) { code }
#else
    #define Debug(code)
#endif
