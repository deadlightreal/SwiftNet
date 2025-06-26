#include "../../src/swift_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DATA_TO_SEND 100000

SwiftNetClientConnection* client;
SwiftNetServer* server;

uint8_t* random_generated_data = NULL;

#define REQUEST_SEND_LARGE_PACKET 0xFF

void client_message_handler(uint8_t* data, SwiftNetPacketClientMetadata* restrict const metadata) {
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

void server_message_handler(uint8_t* data, SwiftNetPacketServerMetadata* restrict const metadata) {
    printf("server got msg %d\n", data[0]);

    if (data[0] == REQUEST_SEND_LARGE_PACKET) {
        swiftnet_server_append_to_packet(server, random_generated_data, DATA_TO_SEND);
        printf("apended\n");

        swiftnet_server_send_packet(server, metadata->sender);
        printf("sent\n");

        swiftnet_server_clear_send_buffer(server);

        printf("sent\n");
    }
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
    swiftnet_client_set_buffer_size(client, DATA_TO_SEND);

    const uint8_t message = REQUEST_SEND_LARGE_PACKET;

    swiftnet_client_append_to_packet(client, &message, sizeof(message));

    swiftnet_client_send_packet(client);

    swiftnet_client_clear_send_buffer(client);

    usleep(10000000);

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    free(random_generated_data);

    return EXIT_FAILURE;
}
