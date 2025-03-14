#pragma once

#include "./initialize_server_socket.h"
#include "./main.h"
#include <stdint.h>

static inline void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data)) {
    SwiftNetMessageHandler = handler;
}

static inline void SwiftNetSetBufferSize(unsigned int newBufferSize) {
    SwiftNetBufferSize = newBufferSize;
}
