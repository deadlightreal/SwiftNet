#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>
#include "swift_net.h"

SwiftNetServer* swiftnet_create_server(const char* const restrict ip_address, const uint16_t port) {
    SwiftNetServer* restrict empty_server = NULL;
    for(uint8_t i = 0; i < MAX_SERVERS; i++) {
        SwiftNetServer* const restrict current_server = &SwiftNetServers[i];
        if(current_server->sockfd != -1) {
            continue;
        }

        empty_server = current_server;

        break;
    }

    SwiftNetErrorCheck(
        if(unlikely(empty_server == NULL)) {
            fprintf(stderr, "Failed to get an empty server\n");
            exit(EXIT_FAILURE);
        }
    )

    empty_server->server_port = port;

    // Create the socket
    empty_server->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (unlikely(empty_server->sockfd < 0)) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(empty_server->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Allocate memory for the packet buffer
    uint8_t* const restrict buffer_pointer = (uint8_t*)malloc(empty_server->buffer_size + sizeof(SwiftNetPacketInfo));
    if(unlikely(buffer_pointer == NULL)) {
        fprintf(stderr, "Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    uint8_t* const restrict data_pointer = buffer_pointer + sizeof(SwiftNetPacketInfo);

    empty_server->packet.packet_buffer_start = buffer_pointer;
    empty_server->packet.packet_data_start = data_pointer;
    empty_server->packet.packet_append_pointer = data_pointer;

    memset(empty_server->pending_messages, 0x00, MAX_PENDING_MESSAGES * sizeof(SwiftNetPendingMessage));
    // Initialize transfer clients to NULL | 0x00

    memset(empty_server->packets_sending, 0x00, MAX_PACKETS_SENDING * sizeof(SwiftNetPacketSending));

    // Create a new thread that will handle all packets received
    pthread_create(&empty_server->handle_packets_thread, NULL, swiftnet_handle_packets, empty_server);

    return empty_server;
}
