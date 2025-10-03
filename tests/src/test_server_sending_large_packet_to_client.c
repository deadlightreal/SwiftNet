#include "../../src/swift_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../config.h"

#define DATA_TO_SEND 100000

SwiftNetClientConnection* client;
SwiftNetServer* server;

uint8_t* random_generated_data = NULL;

#define REQUEST_SEND_LARGE_PACKET 0xFF

void client_message_handler(SwiftNetClientPacketData* restrict const packet_data) {
    for(uint32_t i = 0; i < packet_data->metadata.data_length; i++) {
        if(random_generated_data[i] != packet_data->data[i]) {
            fprintf(stderr, "invalid byte at index: %d\ndata received: %d\ndata sent: %d\n", i, packet_data->data[i], random_generated_data[i]);
            swiftnet_server_cleanup(server);
            swiftnet_client_cleanup(client);
            free(random_generated_data);
            exit(EXIT_FAILURE);
        }
    }

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);
    free(random_generated_data);
    exit(EXIT_SUCCESS);
}

void server_message_handler(SwiftNetServerPacketData* restrict const packet_data) {
    if (packet_data->data[0] == REQUEST_SEND_LARGE_PACKET) {
        SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(DATA_TO_SEND);

        swiftnet_server_append_to_packet(server, random_generated_data, DATA_TO_SEND, &buffer);

        swiftnet_server_send_packet(server, &buffer, packet_data->metadata.sender);

        swiftnet_server_destroy_packet_buffer(&buffer);
    }
}

int main() {
    swiftnet_add_debug_flags(DEBUG_PACKETS_RECEIVING | DEBUG_PACKETS_SENDING | DEBUG_INITIALIZATION | DEBUG_LOST_PACKETS);

    random_generated_data = malloc(DATA_TO_SEND);
    if (random_generated_data == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        return EXIT_FAILURE;
    }

    for(uint32_t i = 0; i < DATA_TO_SEND; i++) {
        random_generated_data[i] = rand();
    }

    swiftnet_initialize();

    server = swiftnet_create_server(8080);
    swiftnet_server_set_message_handler(server, server_message_handler);

    client = swiftnet_create_client(IP_ADDRESS, 8080);
    swiftnet_client_set_message_handler(client, client_message_handler);

    const uint8_t message = REQUEST_SEND_LARGE_PACKET;

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(sizeof(message));

    swiftnet_client_append_to_packet(client, &message, sizeof(message), &buffer);

    swiftnet_client_send_packet(client, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);

    usleep(10000000);

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    free(random_generated_data);

    return EXIT_FAILURE;
}
