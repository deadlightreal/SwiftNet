#pragma once

#include "./initialize_server_socket.h"
#include "./main.h"
#include <stdint.h>

// Set the handler for incoming packets/messages on the server
static inline void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data)) {
    SwiftNetMessageHandler = handler;
}

// Change the buffer size of a package
static inline void SwiftNetSetBufferSize(unsigned int newBufferSize) {
    SwiftNetBufferSize = newBufferSize;
}
