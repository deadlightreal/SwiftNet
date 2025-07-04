#include "../../src/swift_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DATA_TO_SEND 100000

SwiftNetClientConnection* client;
SwiftNetServer* server;

uint8_t* random_generated_data = NULL;

void client_message_handler(uint8_t* data, SwiftNetPacketClientMetadata* restrict const metadata) {
}

void server_message_handler(uint8_t* data, SwiftNetPacketServerMetadata* restrict const metadata) {
    for(uint32_t i = 0; i < metadata->data_length; i++) {
        if(random_generated_data[i] != data[i]) {
            fprintf(stderr, "invalid byte at index: %d\ndata received: %d\ndata sent: %d\n", i, data[i], random_generated_data[i]);
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

int main() {
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

    client = swiftnet_create_client("127.0.0.1", 8080);
    swiftnet_client_set_message_handler(client, client_message_handler);

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(DATA_TO_SEND);

    swiftnet_client_append_to_packet(client, random_generated_data, DATA_TO_SEND, &buffer);

    swiftnet_client_send_packet(client, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);

    usleep(10000000);

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    free(random_generated_data);

    return EXIT_FAILURE;
}
