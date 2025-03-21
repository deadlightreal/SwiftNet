#include "swift_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// These functions append data to a packet buffer and advance the current pointer by the data size.

static inline void AppendToPacketServer(SwiftNetServer* server, void* data, unsigned int dataSize) {
    memcpy(server->packetAppendPointer, data, dataSize);

    server->packetAppendPointer += dataSize;
};

static inline void AppendToPacketClient(SwiftNetClientConnection* client, void* data, unsigned int dataSize) {
    memcpy(client->packetAppendPointer, data, dataSize);

    client->packetAppendPointer += dataSize;
}

void SwiftNetAppendToPacket(void* connection, void* data, unsigned int dataSize) {
    SwiftNetDebug(
        if(unlikely(connection == NULL || data == NULL || dataSize == 0)) {
            fprintf(stderr, "Error: Invalid arguments given to function append to packet.\n");
            exit(EXIT_FAILURE);
        }
    )

    SwiftNetServerCode(
        AppendToPacketServer(connection, data, dataSize);
    )

    SwiftNetClientCode(
        AppendToPacketClient(connection, data, dataSize);
    )
}
