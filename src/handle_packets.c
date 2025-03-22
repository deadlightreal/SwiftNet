#include "swift_net.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>

static inline void HandlePacketsServer(SwiftNetServer* server) {
    while(1) {
        const unsigned int size = sizeof(ClientInfo) + sizeof(struct ip) + server->bufferSize;

        uint8_t buffer[size];

        struct sockaddr_in clientAddress;
        socklen_t clientAddressLen = sizeof(clientAddress);
    
        int messageSize = recvfrom(server->sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddress, &clientAddressLen);
        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetDebug(
            if(unlikely(server->packetHandler == NULL)) {
                fprintf(stderr, "Message Handler not net on server!!!!\n");
                exit(EXIT_FAILURE);
            }
        )

        struct ip ipHeader;
        memcpy(&ipHeader, buffer, sizeof(struct ip));
    
        ClientInfo clientInfo;
        memcpy(&clientInfo, buffer + sizeof(struct ip), sizeof(ClientInfo));

        clientAddress.sin_port = clientInfo.source_port;

        ClientAddrData sender = {clientAddress, clientAddressLen};
    
        // Check if the packet is meant to be for this server
        if(clientInfo.destination_port != server->server_port) {
            continue;
        }
    
        // Execute function set by user
        server->packetHandler(buffer + sizeof(ClientInfo) + sizeof(struct ip), sender);
    }
}

static inline void HandlePacketsClient(SwiftNetClientConnection* client) {
    while(1) {
        uint8_t buffer[client->bufferSize + sizeof(ClientInfo) + sizeof(struct ip)];
    
        int messageSize = recvfrom(client->sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
    
        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetDebug(
            if(unlikely(client->packetHandler == NULL)) {
                fprintf(stderr, "Message Handler not set on client!!!!\n");
                exit(EXIT_FAILURE);
            }
        )

        struct ip ipHeader;
        memcpy(&ipHeader, buffer, sizeof(struct ip));
    
        ClientInfo clientInfo;
        memcpy(&clientInfo, buffer + sizeof(struct ip), sizeof(ClientInfo));
    
        // Check if the packet is meant to be for this server
        if(clientInfo.destination_port != client->clientInfo.source_port) {
            continue;
        }

        // Execute function set by user
        client->packetHandler(buffer + sizeof(ClientInfo) + sizeof(struct ip));
    }
}

void* SwiftNetHandlePackets(void* connection) {
 
    SwiftNetServerCode(
        HandlePacketsServer(connection);
    )

    SwiftNetClientCode(
        HandlePacketsClient(connection);
    )

    return NULL;
}
