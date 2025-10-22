#include "../../src/swift_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../config.h"

#define DATA_TO_SEND 100000

SwiftNetClientConnection* client;
SwiftNetServer* server;

uint32_t* random_generated_data = NULL;

void client_message_handler(SwiftNetClientPacketData* restrict const packet_data) {
}

void server_message_handler(SwiftNetServerPacketData* restrict const packet_data) {
    for(uint32_t i = 0; i < packet_data->metadata.data_length / 4; i++) {
        const uint32_t data_received = *(uint32_t*)swiftnet_server_read_packet(packet_data, 4);

        if(random_generated_data[i] != data_received) {
            fprintf(stderr, "invalid byte at index: %d\ndata received: %d\ndata sent: %d\n", i, packet_data->data[i], random_generated_data[i]);
            printf("next bytes: %d %d %d before bytes: %d %d %d\n", packet_data->data[i+1], packet_data->data[i+2], packet_data->data[i+3], packet_data->data[i-1], packet_data->data[i-2], packet_data->data[i-3]);
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

    for(uint32_t i = 0; i < DATA_TO_SEND / 4; i++) {
        random_generated_data[i] = i;
    }

    swiftnet_initialize();

    server = swiftnet_create_server(8080);
    swiftnet_server_set_message_handler(server, server_message_handler);

    client = swiftnet_create_client(IP_ADDRESS, 8080);
    swiftnet_client_set_message_handler(client, client_message_handler);

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(DATA_TO_SEND);

    swiftnet_client_append_to_packet(random_generated_data, DATA_TO_SEND, &buffer);

    swiftnet_client_send_packet(client, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);

    usleep(10000000);

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    free(random_generated_data);

    swiftnet_cleanup();

    return EXIT_FAILURE;
}
