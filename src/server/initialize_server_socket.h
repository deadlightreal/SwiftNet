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

        Debug(
            if(SwiftNetMessageHandler == NULL) {
                perror("Message Handler Not Set!!!!\n");
                exit(EXIT_FAILURE);
            }
        )

        for(unsigned int i = 0; i < size; i++) {
            printf("%d ", buffer[i]);
        }

        printf("\n");

        ClientInfo clientInfo;
        memcpy(&clientInfo, buffer + 20, sizeof(ClientInfo));

        printf("client port: %d\nserver port: %d\n", clientInfo.port, serverSocketData.port);

        if(clientInfo.port != serverSocketData.port) {
            continue;
        }

        SwiftNetMessageHandler(buffer + sizeof(ClientInfo) + 20);
    }

    return NULL;
}

static inline void SwiftNetCreateServer(char* ip_address, uint16_t port) {
    struct sockaddr_in server_addr;

    serverSocketData.port = port;

    serverSocketData.sockfd = socket(AF_INET, SOCK_RAW, 253);
    if (serverSocketData.sockfd < 0) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(serverSocketData.sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    pthread_create(&handlePacketsThread, NULL, SwiftNetHandlePackets, NULL);
}
