#include "swift_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// These functions append data to a packet buffer and advance the current pointer by the data size.

static inline void ValidateArgs(void* con, void* data, unsigned int dataSize) {
    if(unlikely(con == NULL || data == NULL || dataSize == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to function append to packet.\n");
        exit(EXIT_FAILURE);
    }
}

SwiftNetServerCode(
void SwiftNetAppendToPacket(SwiftNetServer* server, void* data, unsigned int dataSize) {
    SwiftNetErrorCheck(
        ValidateArgs(server, data, dataSize);
    )

    memcpy(server->packet.packetAppendPointer, data, dataSize);

    server->packet.packetAppendPointer += dataSize;
}
)

SwiftNetClientCode(
void SwiftNetAppendToPacket(SwiftNetClientConnection* client, void* data, unsigned int dataSize) {
    SwiftNetErrorCheck(
        ValidateArgs(client, data, dataSize);
    )

    memcpy(client->packet.packetAppendPointer, data, dataSize);

    client->packet.packetAppendPointer += dataSize;
}
)
