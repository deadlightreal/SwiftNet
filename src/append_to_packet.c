#include "swift_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "internal/internal.h"

// These functions append data to a packet buffer and advance the current pointer by the data size.

static inline void validate_args(const void* const data, const uint32_t data_size) {
    if(unlikely(data == NULL || data_size == 0)) {
        PRINT_ERROR("Error: Invalid arguments given");
        exit(EXIT_FAILURE);
    }
}

static inline void append_data(uint8_t** const append_pointer, const void* const data, const uint32_t data_size) {
    memcpy(*append_pointer, data, data_size);

    (*append_pointer) += data_size;
}

void swiftnet_client_append_to_packet(const void* const data, const uint32_t data_size, struct SwiftNetPacketBuffer* const packet) {
    #ifdef SWIFT_NET_ERROR
        validate_args(data, data_size);
    #endif

    append_data(&packet->packet_append_pointer, data, data_size);
}

void swiftnet_server_append_to_packet(const void* const data, const uint32_t data_size, struct SwiftNetPacketBuffer* const packet) {
    #ifdef SWIFT_NET_ERROR
        validate_args(data, data_size);
    #endif

    append_data(&packet->packet_append_pointer, data, data_size);
}
