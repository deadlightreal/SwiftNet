#include "../../src/swift_net.h"
#include "../config.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

SwiftNetServer* server;
SwiftNetClientConnection* client;

const char* restrict const message = "hello";

void client_message_handler(SwiftNetClientPacketData* restrict const packet_data) {
    printf("Got message on client for some reason\n");

    swiftnet_server_cleanup(server);
    swiftnet_client_cleanup(client);

    swiftnet_cleanup();

    exit(EXIT_SUCCESS);
}

void server_message_handler(SwiftNetServerPacketData* restrict const packet_data) {
    uint8_t* data = swiftnet_server_read_packet(packet_data, packet_data->metadata.data_length);

    if(memcmp(data, message, packet_data->metadata.data_length) == 0) {
        SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(strlen(message));

        swiftnet_server_append_to_packet(message, strlen(message), &buffer);

        swiftnet_server_make_response(server, packet_data, &buffer);

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

    swiftnet_client_append_to_packet(message, strlen(message), &buffer);

    SwiftNetClientPacketData* packet_data = swiftnet_client_make_request(client, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);

    if (packet_data->metadata.data_length != strlen(message)) {
        swiftnet_server_cleanup(server);
        swiftnet_client_cleanup(client);

        swiftnet_cleanup();

        exit(EXIT_FAILURE);
    }

    uint8_t* data = swiftnet_client_read_packet(packet_data, packet_data->metadata.data_length);

    if(memcmp(data, message, packet_data->metadata.data_length) == 0) {
        swiftnet_server_cleanup(server);
        swiftnet_client_cleanup(client);

        swiftnet_cleanup();

        exit(EXIT_SUCCESS);
    } else {
        swiftnet_server_cleanup(server);
        swiftnet_client_cleanup(client);

        swiftnet_cleanup();

        return 0;
    }
}
