#include "internal/internal.h"
#include "swift_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Set the handler for incoming packets/messages on the server or client

static inline void swiftnet_validate_new_handler(const void* const new_handler, const char* const restrict caller) {
    #ifdef SWIFT_NET_ERROR
        if(unlikely(new_handler == NULL)) {
            fprintf(stderr, "Error: Invalid arguments given to function: %s\n", caller);
            exit(EXIT_FAILURE);
        }
    #endif
}

void swiftnet_client_set_message_handler(SwiftNetClientConnection* const client, void (* const new_handler)(SwiftNetClientPacketData* restrict const)) {
    swiftnet_validate_new_handler(new_handler, __func__);

    atomic_store(&client->packet_handler, new_handler);
}

void swiftnet_server_set_message_handler(SwiftNetServer* const server, void (* const new_handler)(SwiftNetServerPacketData* const)) {
    swiftnet_validate_new_handler(new_handler, __func__);

    atomic_store(&server->packet_handler, new_handler);
}

// Read packet data into buffers

void* swiftnet_client_read_packet(SwiftNetClientPacketData* const packet_data, const uint32_t data_size) {
    const uint32_t data_already_read = (packet_data->current_pointer - packet_data->data) + data_size;
    if (data_already_read > packet_data->metadata.data_length) {
        fprintf(stderr, "Error: Tried to read more data than there actually is\n");
        return NULL;
    }

    void* const ptr = packet_data->current_pointer;

    packet_data->current_pointer += data_size;

    return ptr;
}

void* swiftnet_server_read_packet(SwiftNetServerPacketData* const packet_data, const uint32_t data_size) {
    const uint32_t data_already_read = (packet_data->current_pointer - packet_data->data) + data_size;
    if (data_already_read > packet_data->metadata.data_length) {
        fprintf(stderr, "Error: Tried to read more data than there actually is\n");
        return NULL;
    }

    void* const ptr = packet_data->current_pointer;

    packet_data->current_pointer += data_size;

    return ptr;
}

void swiftnet_client_destory_packet_data(SwiftNetClientPacketData* const packet_data) {
    allocator_free(&client_packet_data_memory_allocator, packet_data);
}

void swiftnet_server_destory_packet_data(SwiftNetServerPacketData* const packet_data) {
    allocator_free(&server_packet_data_memory_allocator, packet_data);
}
