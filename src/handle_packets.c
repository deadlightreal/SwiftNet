#include "swift_net.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>

SwiftNetServerCode(
void* SwiftNetHandlePackets(void* serverVoid) {
    SwiftNetServer* server = (SwiftNetServer*)serverVoid;

    while(1) {
        const unsigned int size = sizeof(ClientInfo) + sizeof(struct ip) + server->bufferSize;

        uint8_t buffer[size];

        struct sockaddr_in clientAddress;
        socklen_t clientAddressLen = sizeof(clientAddress);
    
        int messageSize = recvfrom(server->sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddress, &clientAddressLen);
        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetErrorCheck(
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

        ClientAddrData sender;
    
        sender.clientAddr = clientAddress;
        sender.clientAddrLen = clientAddressLen;

        server->packet.packetDataLen = messageSize;

        // Check if the packet is meant to be for this server
        if(clientInfo.destination_port != server->server_port) {
            continue;
        }
    
        // Execute function set by user
        server->packetHandler(buffer + sizeof(ClientInfo) + sizeof(struct ip), sender);
    }

    return NULL;
}
)

SwiftNetClientCode(
void* SwiftNetHandlePackets(void* clientVoid) {
    SwiftNetClientConnection* client = (SwiftNetClientConnection*)clientVoid;

    while(1) {
        uint8_t buffer[client->bufferSize + sizeof(ClientInfo) + sizeof(struct ip)];
    
        int messageSize = recvfrom(client->sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
    
        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetErrorCheck(
            if(unlikely(client->packetHandler == NULL)) {
                fprintf(stderr, "Message Handler not set on client!!!!\n");
                exit(EXIT_FAILURE);
            }
        )

        struct ip ipHeader;
        memcpy(&ipHeader, buffer, sizeof(struct ip));
    
        ClientInfo clientInfo;
        memcpy(&clientInfo, buffer + sizeof(struct ip), sizeof(ClientInfo));

        client->packet.packetDataLen = messageSize;
    
        // Check if the packet is meant to be for this server
        if(clientInfo.destination_port != client->clientInfo.source_port) {
            continue;
        }

        // Execute function set by user
        client->packetHandler(buffer + sizeof(ClientInfo) + sizeof(struct ip));
    }

    return NULL;
}
)
