#include "swift_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Set the handler for incoming packets/messages on the server or client

SwiftNetServerCode(
void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data, ClientAddrData sender), SwiftNetServer* server) {
    SwiftNetDebug(
        if(unlikely(handler == NULL)) {
            fprintf(stderr, "Error: Invalid arguments given to function set message handler.\n");
            exit(EXIT_FAILURE);
        }
    )

    server->packetHandler = handler;
}
)

SwiftNetClientCode(
void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data), SwiftNetClientConnection* client) {
    SwiftNetDebug(
        if(unlikely(handler == NULL)) {
            fprintf(stderr, "Error: Invalid arguments given to function set message handler.\n");
            exit(EXIT_FAILURE);
        }
    )

    client->packetHandler = handler;
}
)

// Adjusts the buffer size for a network packet, reallocating memory as needed.

static inline void SetBufferSizeServer(unsigned int newBufferSize, SwiftNetServer* server) {
    server->bufferSize = newBufferSize;

    unsigned int currentDataPosition = server->packetAppendPointer - server->packetDataStart;

    uint8_t* newDataPointer = realloc(server->packetBufferStart, newBufferSize);

    server->packetBufferStart = newDataPointer;
    server->packetDataStart = newDataPointer + sizeof(ClientInfo);
    server->packetAppendPointer = server->packetDataStart + currentDataPosition;
}

static inline void SetBufferSizeClient(unsigned int newBufferSize, SwiftNetClientConnection* client) {
    client->bufferSize = newBufferSize;

    unsigned int currentDataPosition = client->packetAppendPointer - client->packetDataStart;

    uint8_t* newDataPointer = realloc(client->packetBufferStart, newBufferSize);

    client->packetBufferStart = newDataPointer;
    client->packetDataStart = newDataPointer + sizeof(ClientInfo);
    client->packetAppendPointer = client->packetDataStart + currentDataPosition;
}

void SwiftNetSetBufferSize(unsigned int newBufferSize, void* connection) {
    if(unlikely(connection == NULL || newBufferSize == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to function set buffer size.\n");
        exit(EXIT_FAILURE);
    }

    SwiftNetServerCode(
        SetBufferSizeServer(newBufferSize, connection);
    )

    SwiftNetClientCode(
        SetBufferSizeClient(newBufferSize, connection);
    )
}
