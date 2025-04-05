#include "swift_net.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>

// Todo: Implement receiving data in chunks - 0x2000 | 8192 bytes -- can manually modify using a function
// How to do it: Save all clients sending data in chunks, and when they send all the data execute the function handler

SwiftNetServerCode(
TransferClient* GetTransferClient(PacketInfo packetInfo, in_addr_t clientAddress, SwiftNetServer* server) {
    const TransferClient zeroedTransferClient = {0x00};

    TransferClient* emptyTransferClient = NULL;

    for(unsigned int i = 0; i < MAX_TRANSFER_CLIENTS; i++) {
        TransferClient* currentTransferClient = &server->transferClients[i];

        if(currentTransferClient->packetInfo.client_info.source_port == packetInfo.client_info.source_port && clientAddress == currentTransferClient->clientAddress) {
            // Found a transfer client struct that is meant for this client
            return currentTransferClient;
        }
    }

    return NULL;
}

void* SwiftNetHandlePackets(void* serverVoid) {
    SwiftNetServer* server = (SwiftNetServer*)serverVoid;

    const unsigned int headerSize = sizeof(PacketInfo) + sizeof(struct ip);
    const unsigned int totalBufferSize = headerSize + server->dataChunkSize;

    uint8_t packetBuffer[totalBufferSize];

    while(1) {
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLen = sizeof(clientAddress);
    
        recvfrom(server->sockfd, packetBuffer, sizeof(packetBuffer), 0, (struct sockaddr *)&clientAddress, &clientAddressLen);

        printf("header: ");
        for(unsigned int i = 0; i < headerSize; i++) {
            printf("0x%02X ", packetBuffer[i]);
        }
        printf("\n");

        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetErrorCheck(
            if(unlikely(server->packetHandler == NULL)) {
                fprintf(stderr, "Message Handler not net on server!!!!\n");
                exit(EXIT_FAILURE);;
            }
        )

        struct ip ipHeader;
        memcpy(&ipHeader, packetBuffer, sizeof(ipHeader));

        PacketInfo packetInfo;
        memcpy(&packetInfo, &packetBuffer[sizeof(ipHeader)], sizeof(PacketInfo));

        // Check if the packet is meant to be for this server
        if(packetInfo.client_info.destination_port != server->server_port) {
            continue;
        }

        if(unlikely(packetInfo.packet_length > server->bufferSize)) {
            fprintf(stderr, "Data received is larger than buffer size!\n");
            exit(EXIT_FAILURE);
        }

        TransferClient* transferClient = GetTransferClient(packetInfo, clientAddress.sin_addr.s_addr, server);

        if(transferClient == NULL) {
            if(packetInfo.packet_length > server->dataChunkSize) {
            
            } else {
                clientAddress.sin_port = packetInfo.client_info.source_port;

                printf("data: ");
                for(unsigned int i = headerSize; i < headerSize + packetInfo.packet_length; i++) {
                    printf("0x%02X ", packetBuffer[i]);
                }
                printf("\n");

                ClientAddrData sender;
    
                sender.clientAddr = clientAddress;
                sender.clientAddrLen = clientAddressLen;

                server->packet.packetDataLen = packetInfo.packet_length;

                // Execute function set by user
                server->packetHandler(packetBuffer + headerSize, sender);
            }
        }
    }

    return NULL;
}
)

SwiftNetClientCode(
void* SwiftNetHandlePackets(void* clientVoid) {
    SwiftNetClientConnection* client = (SwiftNetClientConnection*)clientVoid;

    while(1) {
        uint8_t* buffer = malloc(client->bufferSize + sizeof(ClientInfo) + sizeof(struct ip));
    
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
