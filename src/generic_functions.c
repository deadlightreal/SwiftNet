#include "swift_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Set the handler for incoming packets/messages on the server or client

void swiftnet_set_message_handler(void (*handler)(uint8_t*, SwiftNetPacketMetadata), CONNECTION_TYPE* const restrict connection) {
    SwiftNetErrorCheck(
        if(unlikely(handler == NULL)) {
            fprintf(stderr, "Error: Invalid arguments given to function set message handler.\n");
            exit(EXIT_FAILURE);
        }
    )

    connection->packet_handler = handler;
}

// Adjusts the buffer size for a network packet, reallocating memory as needed.
static inline void validate_set_buffer_size_args(const unsigned int size, const CONNECTION_TYPE* const restrict con) {
    if(unlikely(con == NULL || size == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to function set buffer size.\n");
        exit(EXIT_FAILURE);
    }
}

void swiftnet_set_buffer_size(const unsigned int new_buffer_size, CONNECTION_TYPE* const restrict connection) {
    SwiftNetErrorCheck(
        validate_set_buffer_size_args(new_buffer_size, connection);
    )

    SwiftNetServerCode(
        SwiftNetPacket* restrict const packet = &connection->packet;
    )

    SwiftNetClientCode(
        SwiftNetPacket* restrict const packet = &connection->packet;
)

    const unsigned int bytes_to_allocate = new_buffer_size + sizeof(SwiftNetPacketInfo);
    const unsigned int current_data_position = packet->packet_append_pointer - packet->packet_data_start;

    uint8_t* restrict const new_data_pointer = realloc(packet->packet_buffer_start, bytes_to_allocate);

    packet->packet_buffer_start = new_data_pointer;
    packet->packet_data_start = new_data_pointer + sizeof(SwiftNetPacketInfo);
    packet->packet_append_pointer = packet->packet_data_start + current_data_position;

    connection->buffer_size = bytes_to_allocate;
}
