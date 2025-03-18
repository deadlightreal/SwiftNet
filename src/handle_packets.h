#pragma once

#include "./main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    void* connection;
    uint8_t mode;
} SwiftNetHandlePacketsArgs;

void* SwiftNetHandlePackets(void* voidArgs) {
    SwiftNetHandlePacketsArgs* args = (SwiftNetHandlePacketsArgs*)voidArgs;
    uint8_t arg_mode = args->mode;
    void* connection = args->connection;

    mode = arg_mode;

    free(args);

    SwiftNetServerCode(
        printf("checking for messages from clients\n");

        SwiftNetServer* Server = (SwiftNetServer*)connection;
    
        while(1) {
            // Skipping first 20 bytes - Ip header
            uint8_t buffer[Server->bufferSize + sizeof(ClientInfo) + 20];

            Server->lastClientAddrData.clientAddrLen = sizeof(Server->lastClientAddrData.clientAddr);
    
            int size = recvfrom(Server->sockfd, buffer, Server->bufferSize + sizeof(ClientInfo), 0, (struct sockaddr *)&Server->lastClientAddrData.clientAddr, &Server->lastClientAddrData.clientAddrLen);

            // Check if user set a function that will execute with the message received as arg
            SwiftNetDebug(
                if(unlikely(Server->packetHandler == NULL)) {
                    perror("Message Handler Not Set!!!!\n");
                    exit(EXIT_FAILURE);
                }
            )
    
            // Deserialize the clientInfo
            ClientInfo clientInfo;
            memcpy(&clientInfo, buffer + 20, sizeof(ClientInfo));

            Server->lastClientAddrData.clientAddr.sin_port = clientInfo.source_port;
    
            // Check if the packet is meant to be for this server
            if(clientInfo.destination_port != Server->server_port) {
                continue;
            }

            SwiftNetDebug(
                printf("s: \n");
                for(unsigned int i = 0; i < size; i++) {
                    printf("%d ", buffer[i]);
                }
                printf("\n");
            )
    
            // Execute function set by user
            Server->packetHandler(buffer + sizeof(ClientInfo) + 20);
        }
    )

    SwiftNetClientCode(
        printf("checking for messages from server\n");

        SwiftNetClientConnection* clientConnection = (SwiftNetClientConnection*)connection;
    
        while(1) {
            // Skipping first 20 bytes - Ip header
            uint8_t buffer[clientConnection->bufferSize + sizeof(ClientInfo) + 20];
    
            int size = recvfrom(clientConnection->sockfd, buffer, clientConnection->bufferSize + sizeof(ClientInfo), 0, NULL, NULL);
    
            // Check if user set a function that will execute with the message received as arg
            SwiftNetDebug(
                if(unlikely(clientConnection->packetHandler == NULL)) {
                    perror("Message Handler Not Set!!!!\n");
                    exit(EXIT_FAILURE);
                }
            )
    
            // Deserialize the clientInfo
            ClientInfo clientInfo;

            memcpy(&clientInfo, buffer + 20, sizeof(ClientInfo));
    
            // Check if the packet is meant to be for this server
            if(clientInfo.destination_port != clientConnection->clientInfo.source_port) {
                continue;
            }

            SwiftNetDebug(
                printf("c: \n");
                for(unsigned int i = 0; i < size; i++) {
                    printf("%d ", buffer[i]);
                }
                printf("\n");
            )
    
            // Execute function set by user
            clientConnection->packetHandler(buffer + sizeof(ClientInfo) + 20);
        }
    )

    return NULL;
}
