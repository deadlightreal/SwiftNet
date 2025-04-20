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

SwiftNetServer* SwiftNetCreateServer(char* ip_address, uint16_t port) {
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
    uint8_t* dataPointer = (uint8_t*)malloc(emptyServer->bufferSize + sizeof(PacketInfo));
    if(unlikely(dataPointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    emptyServer->packet.packetBufferStart = dataPointer;
    emptyServer->packet.packetDataStart = dataPointer + sizeof(PacketInfo);
    emptyServer->packet.packetAppendPointer = emptyServer->packet.packetDataStart;
    emptyServer->packet.packetReadPointer = emptyServer->packet.packetDataStart;

    memset(emptyServer->transferClients, 0x00, MAX_TRANSFER_CLIENTS * sizeof(TransferClient));
    // Initialize transfer clients to NULL | 0x00

    memset(emptyServer->packets_sending, 0x00, MAX_PACKETS_SENDING * sizeof(PacketSending));

    // Create a new thread that will handle all packets received
    pthread_create(&emptyServer->handlePacketsThread, NULL, SwiftNetHandlePackets, emptyServer);

    return emptyServer;
}
