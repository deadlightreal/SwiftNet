#include "swift_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// These functions append data to a packet buffer and advance the current pointer by the data size.

static inline void ValidateArgs(void* con, void* data, unsigned int data_size) {
    if(unlikely(con == NULL || data == NULL || data_size == 0)) {
        fprintf(stderr, "Error: Invalid arguments given to function append to packet.\n");
        exit(EXIT_FAILURE);
    }
}

void SwiftNetAppendToPacket(CONNECTION_TYPE* connection, void* data, unsigned int data_size) {
    SwiftNetErrorCheck(
        ValidateArgs(connection, data, data_size);
    )

    memcpy(connection->packet.packet_append_pointer, data, data_size);

    connection->packet.packet_append_pointer += data_size;
}
