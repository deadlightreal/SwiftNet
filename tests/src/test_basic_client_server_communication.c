#include "../../src/swift_net.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

SwiftNetServer* server;
SwiftNetClientConnection* client;

const char* restrict const message = "hello";

void client_message_handler(uint8_t* data, SwiftNetPacketClientMetadata* restrict const metadata) {
    if(memcmp(data, message, metadata->data_length) == 0) {
        swiftnet_server_cleanup(server);
        swiftnet_client_cleanup(client);
        exit(EXIT_SUCCESS);
    }
}

void server_message_handler(uint8_t* data, SwiftNetPacketServerMetadata* restrict const metadata) {
    if(memcmp(data, message, metadata->data_length) == 0) {
        swiftnet_server_append_to_packet(server, message, strlen(message));

        swiftnet_server_send_packet(server, metadata->sender);

        swiftnet_server_clear_send_buffer(server);
    };
}

int main() {
    swiftnet_initialize();

    server = swiftnet_create_server(8080);
    swiftnet_server_set_message_handler(server, server_message_handler);

    client = swiftnet_create_client("127.0.0.1", 8080);
    swiftnet_client_set_message_handler(client, client_message_handler);

    swiftnet_client_append_to_packet(client, message, strlen(message));

    swiftnet_client_send_packet(client);

    swiftnet_client_clear_send_buffer(client);

    usleep(2000000);

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    return -1;
}
