#include "swift_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Set the handler for incoming packets/messages on the server or client

void SwiftNetSetMessageHandler(HANDLER_TYPE(handler), CONNECTION_TYPE* connection) {
    SwiftNetErrorCheck(
        if(unlikely(handler == NULL)) {
            fprintf(stderr, "Error: Invalid arguments given to function set message handler.\n");
            exit(EXIT_FAILURE);
        }
    )

    connection->packet_handler = handler;
}

// Adjusts the buffer size for a network packet, reallocating memory as needed.
static inline void ValidateSetBufferSizeArgs(unsigned int size, void* con) {
    if(unlikely(con == NULL || size == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to function set buffer size.\n");
        exit(EXIT_FAILURE);
    }
}

void SwiftNetSetBufferSize(unsigned int new_buffer_size, CONNECTION_TYPE* connection) {
    SwiftNetErrorCheck(
        ValidateSetBufferSizeArgs(new_buffer_size, connection);
    )

    SwiftNetServerCode(
        Packet* packet = &connection->packet;
    )

    SwiftNetClientCode(
        Packet* packet = &connection->packet;
    )

    connection->buffer_size = new_buffer_size;

    unsigned int currentDataPosition = packet->packet_append_pointer - packet->packet_data_start;
    unsigned int currentReadPosition = packet->packet_read_pointer - packet->packet_data_start;

    uint8_t* newDataPointer = realloc(packet->packet_buffer_start, new_buffer_size);

    packet->packet_buffer_start = newDataPointer;
    packet->packet_data_start = newDataPointer + sizeof(PacketInfo);
    packet->packet_append_pointer = packet->packet_data_start + currentDataPosition;
    packet->packet_read_pointer = packet->packet_data_start + currentReadPosition;
}
