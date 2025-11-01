#include "../../src/swift_net.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../config.h"

#define MAX_ITERATIONS 5000

SwiftNetServer* server;
SwiftNetClientConnection* client;

SwiftNetServer* second_server;
SwiftNetClientConnection* second_client;

#define FIRST_PORT 7070
#define SECOND_PORT 8080

_Atomic bool first_port_response = false;
_Atomic bool second_port_response = false;

const char* const message = "hello";

void client_message_handler(SwiftNetClientPacketData* const packet_data) {
    uint8_t* data = swiftnet_client_read_packet(packet_data, packet_data->metadata.data_length);

    if(memcmp(data, message, packet_data->metadata.data_length) == 0) {
        if (packet_data->metadata.port_info.source_port == FIRST_PORT) {
            atomic_store(&first_port_response, true);
        } else if (packet_data->metadata.port_info.source_port == SECOND_PORT) {
            atomic_store(&second_port_response, true);
        }
    }
}

void server_message_handler(SwiftNetServerPacketData* const packet_data) {
    uint8_t* data = swiftnet_server_read_packet(packet_data, packet_data->metadata.data_length);

    if(memcmp(data, message, packet_data->metadata.data_length) == 0) {
        SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(strlen(message));

        SwiftNetServer* srvr = NULL;
        if (packet_data->metadata.port_info.destination_port == FIRST_PORT) {
            srvr = server;
        } else if (packet_data->metadata.port_info.destination_port == SECOND_PORT) {
            srvr = second_server;
        }

        swiftnet_server_append_to_packet(message, strlen(message), &buffer);

        swiftnet_server_send_packet(srvr, &buffer, packet_data->metadata.sender);

        swiftnet_server_destroy_packet_buffer(&buffer);
    };
}

int main() {
    swiftnet_add_debug_flags(DEBUG_PACKETS_RECEIVING | DEBUG_PACKETS_SENDING | DEBUG_INITIALIZATION | DEBUG_LOST_PACKETS);

    swiftnet_initialize();

    server = swiftnet_create_server(FIRST_PORT);
    swiftnet_server_set_message_handler(server, server_message_handler);

    client = swiftnet_create_client(IP_ADDRESS, FIRST_PORT);
    swiftnet_client_set_message_handler(client, client_message_handler);

    second_server = swiftnet_create_server(SECOND_PORT);
    swiftnet_server_set_message_handler(second_server, server_message_handler);

    second_client = swiftnet_create_client(IP_ADDRESS, SECOND_PORT);
    swiftnet_client_set_message_handler(second_client, client_message_handler);

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(strlen(message));

    swiftnet_client_append_to_packet(message, strlen(message), &buffer);

    swiftnet_client_send_packet(client, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);

    SwiftNetPacketBuffer second_buffer = swiftnet_client_create_packet_buffer(strlen(message));

    swiftnet_client_append_to_packet(message, strlen(message), &second_buffer);

    swiftnet_client_send_packet(second_client, &second_buffer);

    swiftnet_client_destroy_packet_buffer(&second_buffer);

    uint32_t iterations = 0;

    while (atomic_load(&first_port_response) == false || atomic_load(&second_port_response) == false) {
        iterations++;

        if (iterations >= MAX_ITERATIONS) {
            swiftnet_server_cleanup(server);
            swiftnet_server_cleanup(second_server);

            swiftnet_client_cleanup(client);
            swiftnet_client_cleanup(second_client);

            swiftnet_cleanup();

            exit(EXIT_FAILURE);
        }

        usleep(1000);
    }

    swiftnet_server_cleanup(server);
    swiftnet_server_cleanup(second_server);

    swiftnet_client_cleanup(client);
    swiftnet_client_cleanup(second_client);

    swiftnet_cleanup();

    exit(EXIT_SUCCESS);
}
