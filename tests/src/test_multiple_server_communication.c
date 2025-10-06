#include "../../src/swift_net.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../config.h"

#define MAX_ITERATIONS 3000

const uint16_t ports_to_test[] = {
    1, 2, 3
};

SwiftNetServer* server;
SwiftNetClientConnection* client;

const char* restrict const message = "hello";

bool received_response_successfully = false;

void client_message_handler(SwiftNetClientPacketData* restrict const packet_data) {
    uint8_t* data = swiftnet_client_read_packet(packet_data, packet_data->metadata.data_length);

    if(memcmp(data, message, packet_data->metadata.data_length) == 0) {
        received_response_successfully = true;
    }
}

void server_message_handler(SwiftNetServerPacketData* restrict const packet_data) {
    uint8_t* data = swiftnet_server_read_packet(packet_data, packet_data->metadata.data_length);

    if(memcmp(data, message, packet_data->metadata.data_length) == 0) {
        SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(strlen(message));

        swiftnet_server_append_to_packet(server, message, strlen(message), &buffer);

        swiftnet_server_send_packet(server, &buffer, packet_data->metadata.sender);

        swiftnet_server_destroy_packet_buffer(&buffer);
    };
}

int main() {
    swiftnet_add_debug_flags(DEBUG_PACKETS_RECEIVING | DEBUG_PACKETS_SENDING | DEBUG_INITIALIZATION | DEBUG_LOST_PACKETS);

    swiftnet_initialize();

    for (uint32_t i = 0; i < (sizeof(ports_to_test) / sizeof(ports_to_test[0])); i++) {
        const uint16_t port_to_test = ports_to_test[i];

        server = swiftnet_create_server(port_to_test);
        swiftnet_server_set_message_handler(server, server_message_handler);

        client = swiftnet_create_client(IP_ADDRESS, port_to_test);
        swiftnet_client_set_message_handler(client, client_message_handler);   

        SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(strlen(message));

        swiftnet_client_append_to_packet(client, message, strlen(message), &buffer);

        swiftnet_client_send_packet(client, &buffer);

        swiftnet_client_destroy_packet_buffer(&buffer);

        uint32_t iterations = 0;

        while (!received_response_successfully) {
            iterations++;

            if (iterations >= MAX_ITERATIONS) {
                swiftnet_client_cleanup(client);
                swiftnet_server_cleanup(server);

                exit(EXIT_FAILURE);
            }

            usleep(1000);
        }

        received_response_successfully = false;

        swiftnet_server_cleanup(server);
        swiftnet_client_cleanup(client);

        server = NULL;
        client = NULL;
    }

    return 0;
}
