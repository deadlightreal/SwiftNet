#include "swift_net.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>

// Todo: Implement receiving data in chunks - 0x2000 | 8192 bytes -- can manually modify using a function
// How to do it: Save all clients sending data in chunks, and when they send all the data execute the function handler

SwiftNetServerCode(
static inline TransferClient* GetTransferClient(PacketInfo packetInfo, in_addr_t clientAddress, SwiftNetServer* server) {
    TransferClient* emptyTransferClient = NULL;

    for(unsigned int i = 0; i < MAX_TRANSFER_CLIENTS; i++) {
        TransferClient* currentTransferClient = &server->transferClients[i];

        if(currentTransferClient->packetInfo.client_info.source_port == packetInfo.client_info.source_port && clientAddress == currentTransferClient->clientAddress && packetInfo.packet_id == currentTransferClient->packetInfo.packet_id) {
            // Found a transfer client struct that is meant for this client
            return currentTransferClient;
        }
    }

    return NULL;
}

static inline TransferClient* CreateNewTransferClient(PacketInfo packetInfo, in_addr_t clientAddress, SwiftNetServer* server) {
    const TransferClient emptyTransferClient = {0x00};

    for(unsigned int i = 0; i < MAX_TRANSFER_CLIENTS; i++) {
        TransferClient* currentTransferClient = &server->transferClients[i];

        if(memcmp(&emptyTransferClient, currentTransferClient, sizeof(TransferClient)) == 0) {
            // Found empty transfer client slot
            currentTransferClient->clientAddress = clientAddress;
            currentTransferClient->packetInfo = packetInfo;
            
            uint8_t* allocatedMem = malloc(packetInfo.packet_length);
            
            currentTransferClient->packetDataStart = allocatedMem;
            currentTransferClient->packetCurrentPointer = allocatedMem;

            return currentTransferClient;
        }
    }

    fprintf(stderr, "Error: exceeded maximum number of transfer clients\n");
    exit(EXIT_FAILURE);
}

static inline void RequestNextChunk(SwiftNetServer* server, ClientAddrData client) {
    PacketInfo packetInfo;
    packetInfo.packet_id = PACKET_INFO_ID_NONE;
    packetInfo.packet_length = 0;
    packetInfo.packet_type = PACKET_TYPE_SEND_NEXT_CHUNK;

    packetInfo.client_info.source_port = server->server_port;
    packetInfo.client_info.destination_port = client.clientAddr.sin_port;

    sendto(server->sockfd, &packetInfo, sizeof(PacketInfo), 0, (const struct sockaddr *)&client.clientAddr, client.clientAddrLen);

    printf("requested next chunk\n");
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

        clientAddress.sin_port = packetInfo.client_info.source_port;

        ClientAddrData sender;
        sender.clientAddr = clientAddress;
        sender.clientAddrLen = clientAddressLen; 

        TransferClient* transferClient = GetTransferClient(packetInfo, clientAddress.sin_addr.s_addr, server);

        if(transferClient == NULL) {
            printf("packet length: %d\n", packetInfo.packet_length);
            if(packetInfo.packet_length > server->dataChunkSize) {
                TransferClient* newlyCreatedTransferClient = CreateNewTransferClient(packetInfo, clientAddress.sin_addr.s_addr, server);

                // Copy the data from buffer to the transfer client
                memcpy(newlyCreatedTransferClient->packetCurrentPointer, &packetBuffer[headerSize], server->dataChunkSize);

                newlyCreatedTransferClient->packetCurrentPointer += server->dataChunkSize;

                RequestNextChunk(server, sender);
            } else {
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
        } else {
            // Found a transfer client
            unsigned int bytesNeededToCompletePacket = packetInfo.packet_length - (transferClient->packetCurrentPointer - transferClient->packetDataStart);

            printf("Data received: %.2f\n", 1.0 - ((float)bytesNeededToCompletePacket / (float)packetInfo.packet_length));
            printf("Bytes remaining: %d\n", bytesNeededToCompletePacket);

            // If this chunk is the last to complpete the packet
            if(bytesNeededToCompletePacket < server->dataChunkSize) {
                memcpy(transferClient->packetCurrentPointer, &packetBuffer[headerSize], bytesNeededToCompletePacket);

                server->packetHandler(transferClient->packetDataStart, sender);

                free(transferClient->packetDataStart);

                memset(transferClient, 0x00, sizeof(TransferClient));

                printf("Finished data receiving\n");
            } else {
                memcpy(transferClient->packetCurrentPointer, &packetBuffer[headerSize], server->dataChunkSize);

                transferClient->packetCurrentPointer += server->dataChunkSize;

                RequestNextChunk(server, sender);
            }
        }
    }

    return NULL;
}
)

SwiftNetClientCode(
void* SwiftNetHandlePackets(void* clientVoid) {
    SwiftNetClientConnection* client = (SwiftNetClientConnection*)clientVoid;
    
    const unsigned int headerSize = sizeof(PacketInfo) + sizeof(struct ip);
    const unsigned int totalBufferSize = headerSize + client->dataChunkSize;

    uint8_t packetBuffer[totalBufferSize];

    printf("checking for messages from server\n");

    while(1) {
        recvfrom(client->sockfd, packetBuffer, sizeof(packetBuffer), 0, NULL, NULL);

        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetErrorCheck(
            if(unlikely(client->packetHandler == NULL)) {
                fprintf(stderr, "Message Handler not set on client!!!!\n");
                exit(EXIT_FAILURE);
            }
        )

        struct ip ipHeader;
        memcpy(&ipHeader, packetBuffer, sizeof(struct ip));
    
        PacketInfo packetInfo;
        memcpy(&packetInfo, &packetBuffer[sizeof(struct ip)], sizeof(PacketInfo));

        client->packet.packetDataLen = packetInfo.packet_length;

        printf("got message from server from %d to %d\n", packetInfo.client_info.source_port, packetInfo.client_info.destination_port);

        // Check if the packet is meant to be for this client
        if(packetInfo.client_info.destination_port != client->clientInfo.source_port) {
            continue;
        }

        if(packetInfo.packet_type == PACKET_TYPE_SEND_NEXT_CHUNK) {
            SwiftNetServerRequestedNextChunk = true;
            continue;
        }

        // Execute function set by user
        client->packetHandler(&packetBuffer[headerSize]);
    }

    return NULL;
}
)
