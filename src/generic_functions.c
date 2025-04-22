#include "swift_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Set the handler for incoming packets/messages on the server or client

void swiftnet_set_message_handler(HANDLER_TYPE(handler), CONNECTION_TYPE* connection) {
    SwiftNetErrorCheck(
        if(unlikely(handler == NULL)) {
            fprintf(stderr, "Error: Invalid arguments given to function set message handler.\n");
            exit(EXIT_FAILURE);
        }
    )

    connection->packet_handler = handler;
}

// Adjusts the buffer size for a network packet, reallocating memory as needed.
static inline void validate_set_buffer_size_args(unsigned int size, void* con) {
    if(unlikely(con == NULL || size == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to function set buffer size.\n");
        exit(EXIT_FAILURE);
    }
}

void swiftnet_set_buffer_size(unsigned int new_buffer_size, CONNECTION_TYPE* connection) {
    SwiftNetErrorCheck(
        validate_set_buffer_size_args(new_buffer_size, connection);
    )

    SwiftNetServerCode(
        SwiftNetPacket* packet = &connection->packet;
    )

    SwiftNetClientCode(
        SwiftNetPacket* packet = &connection->packet;
    )

    connection->buffer_size = new_buffer_size;

    unsigned int currentDataPosition = packet->packet_append_pointer - packet->packet_data_start;
    unsigned int currentReadPosition = packet->packet_read_pointer - packet->packet_data_start;

    uint8_t* newDataPointer = realloc(packet->packet_buffer_start, new_buffer_size);

    packet->packet_buffer_start = newDataPointer;
    packet->packet_data_start = newDataPointer + sizeof(SwiftNetPacketInfo);
    packet->packet_append_pointer = packet->packet_data_start + currentDataPosition;
    packet->packet_read_pointer = packet->packet_data_start + currentReadPosition;
}
