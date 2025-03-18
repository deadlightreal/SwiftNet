#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "./main.h"
#include "./handle_packets.h"

// Create the socket, and set client and server info
SwiftNetClientConnection* SwiftNetCreateClient(char* ip_address, int port) {
    SwiftNetClientConnection* emptyConnection = NULL;
    for(uint8_t i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        SwiftNetClientConnection* currentConnection = &SwiftNetClientConnections[i];
        if(currentConnection->sockfd != -1) {
            continue;
        }

        emptyConnection = currentConnection;

        break;
    }

    SwiftNetDebug(
        if(unlikely(emptyConnection == NULL)) {
            perror("Failed to get an empty connection\n");
            exit(EXIT_FAILURE);
        }
    )

    emptyConnection->sockfd = socket(AF_INET, SOCK_RAW, 253);
    if(unlikely(emptyConnection->sockfd < 0)) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    uint16_t clientPort = rand();

    printf("client port: %d\n", clientPort);

    emptyConnection->clientInfo.destination_port = port;
    emptyConnection->clientInfo.source_port = clientPort;
    emptyConnection->packetHandler = NULL;
    emptyConnection->bufferSize = 1024;

    uint8_t* dataPointer = (uint8_t*)malloc(emptyConnection->bufferSize + sizeof(ClientInfo));
    if(unlikely(dataPointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    emptyConnection->packetClientInfoPointer = dataPointer;
    emptyConnection->packetDataStartPointer = dataPointer + sizeof(ClientInfo);
    emptyConnection->packetDataCurrentPointer = emptyConnection->packetDataStartPointer;

    memset(&emptyConnection->server_addr, 0, sizeof(emptyConnection->server_addr));
    emptyConnection->server_addr.sin_family = AF_INET;
    emptyConnection->server_addr.sin_port = htons(port);
    emptyConnection->server_addr.sin_addr.s_addr = inet_addr(ip_address);

    SwiftNetHandlePacketsArgs* threadArgs = (SwiftNetHandlePacketsArgs*)malloc(sizeof(SwiftNetHandlePacketsArgs));
    if(threadArgs == NULL) {
        perror("Failed to allocate memory for thread args\n");
        exit(EXIT_FAILURE);
    }

    threadArgs->mode = SWIFT_NET_CLIENT_MODE;
    threadArgs->connection = emptyConnection;

    pthread_create(&emptyConnection->handlePacketsThread, NULL, SwiftNetHandlePackets, threadArgs);

    return emptyConnection;
}
