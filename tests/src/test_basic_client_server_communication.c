#include "../../src/swift_net.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../config.h"

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
        SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(strlen(message));

        swiftnet_server_append_to_packet(server, message, strlen(message), &buffer);

        swiftnet_server_send_packet(server, &buffer, metadata->sender);

        swiftnet_server_destroy_packet_buffer(&buffer);
    };
}

int main() {
    swiftnet_add_debug_flags(DEBUG_PACKETS_RECEIVING | DEBUG_PACKETS_SENDING | DEBUG_INITIALIZATION | DEBUG_LOST_PACKETS);

    swiftnet_initialize();

    server = swiftnet_create_server(8080);
    swiftnet_server_set_message_handler(server, server_message_handler);

    client = swiftnet_create_client(IP_ADDRESS, 8080);
    swiftnet_client_set_message_handler(client, client_message_handler);

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(strlen(message));

    swiftnet_client_append_to_packet(client, message, strlen(message), &buffer);

    swiftnet_client_send_packet(client, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);

    usleep(10000000);

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    return -1;
}
