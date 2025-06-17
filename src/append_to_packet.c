#include "swift_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// These functions append data to a packet buffer and advance the current pointer by the data size.

static inline void validate_args(const void* const restrict con, const void* const restrict data, const uint32_t data_size, const char* const restrict caller) {
    if(unlikely(con == NULL || data == NULL || data_size == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to: %s.\n", caller);
        exit(EXIT_FAILURE);
    }
}

static inline void append_data(uint8_t** restrict const append_pointer, const void* restrict const data, const uint32_t data_size) {
    memcpy(*append_pointer, data, data_size);

    (*append_pointer) += data_size;
}

void swiftnet_client_append_to_packet(SwiftNetClientConnection* const restrict client, const void* const restrict data, const uint32_t data_size) {
    SwiftNetErrorCheck(
        validate_args(client, data, data_size, __func__);
    )

    append_data(&client->packet.packet_append_pointer, data, data_size);
}

void swiftnet_server_append_to_packet(SwiftNetServer* const restrict server, const void* const restrict data, const uint32_t data_size) {
    SwiftNetErrorCheck(
        validate_args(server, data, data_size, __func__);
    )

    append_data(&server->packet.packet_append_pointer, data, data_size);
}
