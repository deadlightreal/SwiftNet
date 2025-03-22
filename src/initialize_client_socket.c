#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "swift_net.h"

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

    emptyConnection->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(unlikely(emptyConnection->sockfd < 0)) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
    
    uint16_t clientPort = rand();

    emptyConnection->clientInfo.destination_port = port;
    emptyConnection->clientInfo.source_port = clientPort;
    emptyConnection->packetHandler = NULL;

    // Base buffer size
    emptyConnection->bufferSize = 1024;

    // Allocate memory for the packet buffer
    uint8_t* dataPointer = (uint8_t*)malloc(emptyConnection->bufferSize + sizeof(ClientInfo));
    if(unlikely(dataPointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    emptyConnection->packetBufferStart = dataPointer;
    emptyConnection->packetDataStart = dataPointer + sizeof(ClientInfo);
    emptyConnection->packetAppendPointer= emptyConnection->packetDataStart;

    memset(&emptyConnection->server_addr, 0, sizeof(emptyConnection->server_addr));
    emptyConnection->server_addr.sin_family = AF_INET;
    emptyConnection->server_addr.sin_port = htons(port);
    emptyConnection->server_addr.sin_addr.s_addr = inet_addr(ip_address);
 
    pthread_create(&emptyConnection->handlePacketsThread, NULL, SwiftNetHandlePackets, emptyConnection);

    return emptyConnection;
}
