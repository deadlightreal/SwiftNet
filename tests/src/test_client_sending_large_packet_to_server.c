#include "../../src/swift_net.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../config.h"

#define DATA_TO_SEND 100000

SwiftNetClientConnection* client;
SwiftNetServer* server;

_Atomic bool finished_sending = false;

uint8_t* random_generated_data = NULL;

void client_message_handler(SwiftNetClientPacketData* const packet_data) {
}

void server_message_handler(SwiftNetServerPacketData* const packet_data) {
    while (atomic_load(&finished_sending) == false) {
        usleep(1000);
    }

    for(uint32_t i = 0; i < packet_data->metadata.data_length / 1; i++) {
        const uint8_t data_received = *(uint8_t*)swiftnet_server_read_packet(packet_data, 1);

        if(random_generated_data[i] != data_received) {
            fprintf(stderr, "invalid byte at index: %d\ndata received: %d\ndata sent: %d\n", i, data_received, random_generated_data[i]);
            fflush(stdout);
            fflush(stderr);
            abort();
            swiftnet_server_cleanup(server);
            swiftnet_client_cleanup(client);
            free(random_generated_data);
            swiftnet_cleanup();
        }
    }

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);
    free(random_generated_data);
    swiftnet_cleanup();
    exit(EXIT_SUCCESS);
}

int main() {
    swiftnet_add_debug_flags(DEBUG_PACKETS_RECEIVING | DEBUG_PACKETS_SENDING | DEBUG_INITIALIZATION | DEBUG_LOST_PACKETS);

    random_generated_data = malloc(DATA_TO_SEND);
    if (random_generated_data == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        return EXIT_FAILURE;
    }

    for(uint32_t i = 0; i < DATA_TO_SEND / 1; i++) {
        random_generated_data[i] = rand();
    }

    swiftnet_initialize();

    server = swiftnet_create_server(8080, LOOPBACK);
    swiftnet_server_set_message_handler(server, server_message_handler);

    client = swiftnet_create_client(IP_ADDRESS, 8080, 2000);
    swiftnet_client_set_message_handler(client, client_message_handler);

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(DATA_TO_SEND);

    swiftnet_client_append_to_packet(random_generated_data, DATA_TO_SEND, &buffer);

    swiftnet_client_send_packet(client, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);

    atomic_store(&finished_sending, true);

    usleep(10000000);

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    free(random_generated_data);

    swiftnet_cleanup();

    return EXIT_FAILURE;
}
