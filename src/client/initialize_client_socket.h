#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../main.h"

// Connection data
typedef struct {
    int sockfd;
    ClientInfo clientInfo;
    struct sockaddr_in server_addr;
} ClientSocketData;

ClientSocketData SwiftNetClientSocketData;

// Create the socket, and set client and server info
void SwiftNetCreateClient(char* ip_address, int port) {
    SwiftNetClientSocketData.sockfd = socket(AF_INET, SOCK_RAW, 253);
    if (SwiftNetClientSocketData.sockfd < 0) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    SwiftNetClientSocketData.clientInfo.port = 8080;

    memset(&SwiftNetClientSocketData.server_addr, 0, sizeof(SwiftNetClientSocketData.server_addr));
    SwiftNetClientSocketData.server_addr.sin_family = AF_INET;
    SwiftNetClientSocketData.server_addr.sin_port = htons(port);
    SwiftNetClientSocketData.server_addr.sin_addr.s_addr = inet_addr(ip_address);
}
