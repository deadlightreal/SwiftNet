#include "swift_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Reads data from the packet and advances the read pointer.

SwiftNetClientCode(
    void SwiftNetReadFromPacket(SwiftNetClientConnection* client, void* ptr, unsigned int size) {
        memcpy(ptr, client->packet.packetReadPointer, size);

        client->packet.packetReadPointer += size;

        SwiftNetErrorCheck(
            unsigned int dataRead = client->packet.packetReadPointer - client->packet.packetDataStart;

            if(unlikely(dataRead > client->packet.packetDataLen)) {
                fprintf(stderr, "Error: Tried to read more data (%du bytes) than the packet has (%du bytes)\n", dataRead, client->packet.packetDataLen);
                exit(EXIT_FAILURE);
            }
        )
    }
)

SwiftNetServerCode(
    void SwiftNetReadFromPacket(SwiftNetServer* server, void* ptr, unsigned int size) {
        memcpy(ptr, server->packet.packetReadPointer, size);

        server->packet.packetReadPointer += size;

        SwiftNetErrorCheck(
            unsigned int dataRead = server->packet.packetReadPointer - server->packet.packetDataStart;

            if(unlikely(dataRead > server->packet.packetDataLen)) {
                fprintf(stderr, "Error: Tried to read more data (%du bytes) than the packet has (%du bytes)\n", dataRead, server->packet.packetDataLen);
                exit(EXIT_FAILURE);
            }
        )
    }
)
