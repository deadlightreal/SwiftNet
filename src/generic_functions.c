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

// Adjusts the buffer size for a network packet, reallocating memory as needed.
static inline void validate_set_buffer_size_args(const uint32_t size, const void* const restrict connection, const char* restrict const caller) {
    if(unlikely(connection == NULL || size == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to function: %s.\n", caller);
        exit(EXIT_FAILURE);
    }
}

static inline void swiftnet_set_buffer_size(const uint32_t new_buffer_size, uint32_t* const restrict buffer_size_pointer, SwiftNetPacket* const restrict packet, const void* const restrict connection, const char* const restrict caller) {
    SwiftNetErrorCheck(
        validate_set_buffer_size_args(new_buffer_size, connection, caller);
    )

    const uint32_t bytes_to_allocate = new_buffer_size + sizeof(SwiftNetPacketInfo);
    const uint32_t current_data_position = packet->packet_append_pointer - packet->packet_data_start;

    uint8_t* restrict const new_data_pointer = realloc(packet->packet_buffer_start, bytes_to_allocate);

    packet->packet_buffer_start = new_data_pointer;
    packet->packet_data_start = new_data_pointer + sizeof(SwiftNetPacketInfo);
    packet->packet_append_pointer = packet->packet_data_start + current_data_position;

    *buffer_size_pointer = bytes_to_allocate;
}

void swiftnet_client_set_buffer_size(SwiftNetClientConnection* const restrict client, const uint32_t new_buffer_size) {
    swiftnet_set_buffer_size(new_buffer_size, &client->buffer_size, &client->packet, client, __func__);
}

void swiftnet_server_set_buffer_size(SwiftNetServer* const restrict server, const uint32_t new_buffer_size) {
    swiftnet_set_buffer_size(new_buffer_size, &server->buffer_size, &server->packet, server, __func__);
}
