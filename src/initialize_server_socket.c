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
    uint8_t* dataPointer = (uint8_t*)malloc(emptyServer->bufferSize + sizeof(ClientInfo));
    if(unlikely(dataPointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    emptyServer->packetBufferStart = dataPointer;
    emptyServer->packetDataStart = dataPointer + sizeof(ClientInfo);
    emptyServer->packetAppendPointer = emptyServer->packetDataStart;

    SwiftNetHandlePacketsArgs* threadArgs = malloc(sizeof(SwiftNetHandlePacketsArgs));
    if(unlikely(threadArgs == NULL)) {
        perror("Failed to allocate memory for thread args\n");
        exit(EXIT_FAILURE);
    }

    threadArgs->connection = emptyServer;
    threadArgs->mode = SWIFT_NET_SERVER_MODE;

    // Create a new thread that will handle all packets received
    pthread_create(&emptyServer->handlePacketsThread, NULL, SwiftNetHandlePackets, threadArgs);

    return emptyServer;
}
