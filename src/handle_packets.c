#include "swift_net.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>

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
            const unsigned int size = sizeof(ClientInfo) + sizeof(struct ip) + Server->bufferSize;

            uint8_t buffer[size];

            Server->lastClientAddrData.clientAddrLen = sizeof(Server->lastClientAddrData.clientAddr);
            printf("sock: %d\n", Server->sockfd);
    
            int messageSize = recvfrom(Server->sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&Server->lastClientAddrData.clientAddr, &Server->lastClientAddrData.clientAddrLen);

            printf("got packet on server side\n");

            // Check if user set a function that will execute with the message received as arg
            SwiftNetDebug(
                if(unlikely(Server->packetHandler == NULL)) {
                    perror("Message Handler Not Set!!!!\n");
                    exit(EXIT_FAILURE);
                }
            )

            struct ip ipHeader;
            memcpy(&ipHeader, buffer, sizeof(struct ip));
    
            // Deserialize the clientInfo
            ClientInfo clientInfo;
            memcpy(&clientInfo, buffer + sizeof(struct ip), sizeof(ClientInfo));

            Server->lastClientAddrData.clientAddr.sin_port = clientInfo.source_port;
    
            // Check if the packet is meant to be for this server
            if(clientInfo.destination_port != Server->server_port) {
                continue;
            }

            SwiftNetDebug(
                printf("s: \n");
                for(unsigned int i = 0; i < messageSize; i++) {
                    printf("%d ", buffer[i]);
                }
                printf("\n");
            )
    
            // Execute function set by user
            Server->packetHandler(buffer + sizeof(ClientInfo) + sizeof(struct ip));
        }
    )

    SwiftNetClientCode(
        printf("checking for messages from server\n");

        SwiftNetClientConnection* clientConnection = (SwiftNetClientConnection*)connection;
    
        while(1) {
            // Skipping first 20 bytes - Ip header
            uint8_t buffer[clientConnection->bufferSize + sizeof(ClientInfo) + sizeof(struct ip)];
    
            int messageSize = recvfrom(clientConnection->sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
    
            // Check if user set a function that will execute with the message received as arg
            SwiftNetDebug(
                if(unlikely(clientConnection->packetHandler == NULL)) {
                    perror("Message Handler Not Set!!!!\n");
                    exit(EXIT_FAILURE);
                }
            )

            // Deserialize ip header
            struct ip ipHeader;
            memcpy(&ipHeader, buffer, sizeof(struct ip));
    
            // Deserialize the clientInfo
            ClientInfo clientInfo;
            memcpy(&clientInfo, buffer + sizeof(struct ip), sizeof(ClientInfo));
    
            // Check if the packet is meant to be for this server
            if(clientInfo.destination_port != clientConnection->clientInfo.source_port) {
                continue;
            }

            SwiftNetDebug(
                printf("c: \n");
                for(unsigned int i = 0; i < messageSize; i++) {
                    printf("%d ", buffer[i]);
                }
                printf("\n");
            )
    
            // Execute function set by user
            clientConnection->packetHandler(buffer + sizeof(ClientInfo) + sizeof(struct ip));
        }
    )

    return NULL;
}
