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
#include <pthread.h>
#include <stdbool.h>
#include "../main.h"
#include "main.h"

typedef struct {
    int sockfd;
    uint16_t port;
} ServerSocketData;

ServerSocketData serverSocketData = {};

pthread_t handlePacketsThread;

void* SwiftNetHandlePackets() {
    while(1) {
        // Skipping first 20 bytes - Ip header
        uint8_t buffer[SwiftNetBufferSize + sizeof(ClientInfo) + 20];

        int size = recvfrom(serverSocketData.sockfd, buffer, SwiftNetBufferSize + sizeof(ClientInfo), 0, NULL, NULL);

        // Check if user set a function that will execute with the message received as arg
        Debug(
            if(SwiftNetMessageHandler == NULL) {
                perror("Message Handler Not Set!!!!\n");
                exit(EXIT_FAILURE);
            }
        )

        Debug(
            for(unsigned int i = 0; i < size; i++) {
                printf("%d ", buffer[i]);
            }
            printf("\n");
        )

        // Deserialize the clientInfo
        ClientInfo clientInfo;
        memcpy(&clientInfo, buffer + 20, sizeof(ClientInfo));

        // Check if the packet is meant to be for this server
        if(clientInfo.port != serverSocketData.port) {
            continue;
        }

        // Execute function set by user
        SwiftNetMessageHandler(buffer + sizeof(ClientInfo) + 20);
    }

    return NULL;
}

static inline void SwiftNetCreateServer(char* ip_address, uint16_t port) {
    struct sockaddr_in server_addr;

    serverSocketData.port = port;

    // Create the socket
    serverSocketData.sockfd = socket(AF_INET, SOCK_RAW, 253);
    if (serverSocketData.sockfd < 0) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    // Set socket options to allow address reuse for the custom protocol
    int opt = 1;
    setsockopt(serverSocketData.sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Create a new thread that will handle all packets received
    pthread_create(&handlePacketsThread, NULL, SwiftNetHandlePackets, NULL);
}
