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

SwiftNetServer* swiftnet_create_server(char* ip_address, uint16_t port) {
    SwiftNetServer* emptyServer = NULL;
    for(uint8_t i = 0; i < MAX_SERVERS; i++) {
        SwiftNetServer* currentServer = &SwiftNetServers[i];
        if(currentServer->sockfd != -1) {
            continue;
        }

        emptyServer = currentServer;

        break;
    }

    SwiftNetErrorCheck(
        if(unlikely(emptyServer == NULL)) {
            perror("Failed to get an empty server\n");
            exit(EXIT_FAILURE);
        }
    )

    struct sockaddr_in server_addr;

    emptyServer->server_port = port;

    // Create the socket
    emptyServer->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (unlikely(emptyServer->sockfd < 0)) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    // Set socket options to allow address reuse for the custom protocol
    int opt = 1;
    setsockopt(emptyServer->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Allocate memory for the packet buffer
    uint8_t* dataPointer = (uint8_t*)malloc(emptyServer->buffer_size + sizeof(SwiftNetPacketInfo));
    if(unlikely(dataPointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    emptyServer->packet.packet_buffer_start = dataPointer;
    emptyServer->packet.packet_data_start = dataPointer + sizeof(SwiftNetPacketInfo);
    emptyServer->packet.packet_append_pointer = emptyServer->packet.packet_data_start;

    memset(emptyServer->transfer_clients, 0x00, MAX_TRANSFER_CLIENTS * sizeof(SwiftNetTransferClient));
    // Initialize transfer clients to NULL | 0x00

    memset(emptyServer->packets_sending, 0x00, MAX_PACKETS_SENDING * sizeof(SwiftNetPacketSending));

    // Create a new thread that will handle all packets received
    pthread_create(&emptyServer->handle_packets_thread, NULL, swiftnet_handle_packets, emptyServer);

    return emptyServer;
}
