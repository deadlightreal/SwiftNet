#pragma once

#include "./initialize_server_socket.h"
#include "./main.h"
#include <stdlib.h>
#include <stdint.h>

// Set the handler for incoming packets/messages on the server
static inline void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data), void* connection) {
    SwiftNetDebug(
        if(unlikely(connection == NULL) || unlikely(handler == NULL)) {
            perror("Provided NULL handler or connection to set packet handler\n");
            exit(EXIT_FAILURE);
        }
    )

    SwiftNetServerCode(
        printf("buf size srvr\n");
        SwiftNetServer* Server = (SwiftNetServer*)connection;
        Server->packetHandler = handler;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* clientConnection = (SwiftNetClientConnection*)connection;
        clientConnection->packetHandler = handler;
    )
}

// Change the buffer size of a package
static inline void SwiftNetSetBufferSize(unsigned int newBufferSize, void* connection) {
    SwiftNetServerCode(
        SwiftNetServer* Server = (SwiftNetServer *)connection;
        Server->bufferSize = newBufferSize;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* clientConnection = (SwiftNetClientConnection *)connection;
        clientConnection->bufferSize = newBufferSize;
    )
}
