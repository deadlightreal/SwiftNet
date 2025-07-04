#include "swift_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Set the handler for incoming packets/messages on the server or client

static inline void swiftnet_validate_new_handler(void* new_handler, const char* const restrict caller) {
    SwiftNetErrorCheck(
        if(unlikely(new_handler == NULL)) {
            fprintf(stderr, "Error: Invalid arguments given to function: %s.\n", caller);
            exit(EXIT_FAILURE);
        }
    )
}

void swiftnet_client_set_message_handler(SwiftNetClientConnection* client, void (*new_handler)(uint8_t*, SwiftNetPacketClientMetadata* restrict const)) {
    swiftnet_validate_new_handler(new_handler, __func__);

    client->packet_handler = new_handler;
}

void swiftnet_server_set_message_handler(SwiftNetServer* server, void (*new_handler)(uint8_t*, SwiftNetPacketServerMetadata* restrict const)) {
    swiftnet_validate_new_handler(new_handler, __func__);

    server->packet_handler = new_handler;
}
