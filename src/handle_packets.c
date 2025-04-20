#include "swift_net.h"
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include "internal/internal.h"

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
)

SwiftNetClientCode(
static inline PendingMessage* GetPendingMessage(PacketInfo* packetInfo, PendingMessage* pending_messages, uint16_t pending_messages_size) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        PendingMessage* current_pending_message = &pending_messages[i];

        if(current_pending_message->packetInfo.packet_id == packetInfo->packet_id && current_pending_message->packetInfo.packet_length == packetInfo->packet_length) {
            return current_pending_message;
        }
    }

    return NULL;
}

static inline PendingMessage* CreateNewPendingMessage(PendingMessage* pending_messages, uint16_t pending_messages_size, PacketInfo* packet_info) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        PendingMessage* current_pending_message = &pending_messages[i];

        if(current_pending_message->packet_current_pointer == NULL) {
            current_pending_message->packetInfo = *packet_info;

            uint8_t* allocated_memory = malloc(packet_info->packet_length);

            current_pending_message->packet_data_start = allocated_memory;
            current_pending_message->packet_current_pointer = allocated_memory;

            return current_pending_message;
        }
    }

    return NULL;
}
)

static inline void RequestNextChunk(CONNECTION_TYPE* connection, uint16_t packet_id EXTRA_REQUEST_NEXT_CHUNK_ARG) {
    PacketInfo packetInfo;
    packetInfo.packet_id = packet_id;
    packetInfo.packet_length = 0;
    packetInfo.packet_type = PACKET_TYPE_SEND_NEXT_CHUNK;

    SwiftNetServerCode(
        const uint16_t source_port = connection->server_port;
        const uint16_t destination_port = target.clientAddr.sin_port;
        const int sockfd = connection->sockfd;
        const struct sockaddr_in* target_sock_addr = &target.clientAddr;
        const socklen_t socklen = target.clientAddrLen;
    )

    SwiftNetClientCode(
        const uint16_t source_port = connection->clientInfo.source_port;
        const uint16_t destination_port = connection->clientInfo.destination_port;
        const int sockfd = connection->sockfd;
        const struct sockaddr_in* target_sock_addr = &connection->server_addr;
        const socklen_t socklen = sizeof(connection->server_addr);
    )

    packetInfo.client_info.source_port = source_port;
    packetInfo.client_info.destination_port = destination_port;

    sendto(sockfd, &packetInfo, sizeof(PacketInfo), 0, (const struct sockaddr *)target_sock_addr, socklen);

}

static inline PacketSending* GetPacketSending(PacketSending* packet_sending_array, const uint16_t size, const uint16_t target_id) {
    for(uint16_t i = 0; i < size; i++) {
        PacketSending* current_packet_sending = &packet_sending_array[i];
        if(current_packet_sending->packet_id == target_id) {
            return current_packet_sending;
        }
    }

    return NULL;
}

void* SwiftNetHandlePackets(void* void_connection) {
    const unsigned int header_size = sizeof(PacketInfo) + sizeof(struct ip);

    SwiftNetServerCode(
        SwiftNetServer* server = (SwiftNetServer*)void_connection;

        void** packetHandler = (void**)&server->packetHandler;

        const unsigned min_mtu = maximum_transmission_unit;
        const int sockfd = server->sockfd;
        const uint16_t source_port = server->server_port;
        const unsigned int* buffer_size = &server->bufferSize;
        PacketSending* packet_sending = server->packets_sending;
        const uint16_t packet_sending_size = MAX_PACKETS_SENDING;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* connection = (SwiftNetClientConnection*)void_connection;

        void** packetHandler = (void**)&connection->packetHandler;

        unsigned int min_mtu = MIN(maximum_transmission_unit, connection->maximum_transmission_unit);
        const int sockfd = connection->sockfd;
        const uint16_t source_port = connection->clientInfo.source_port;
        const unsigned int* buffer_size = &connection->bufferSize;
        PacketSending* packet_sending = connection->packets_sending;
        const uint16_t packet_sending_size = MAX_PACKETS_SENDING;
    )

    const unsigned int total_buffer_size = header_size + min_mtu;

    uint8_t packet_buffer[total_buffer_size];

    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);

    while(1) {
        recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *)&client_address, &client_address_len);

        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetErrorCheck(
            if(unlikely(*packetHandler == NULL)) {
                fprintf(stderr, "Message Handler not set!!\n");
                exit(EXIT_FAILURE);;
            }
        )

        struct ip ip_header;
        memcpy(&ip_header, packet_buffer, sizeof(ip_header));

        PacketInfo packet_info;
        memcpy(&packet_info, &packet_buffer[sizeof(ip_header)], sizeof(PacketInfo));

        // Check if the packet is meant to be for this server
        if(packet_info.client_info.destination_port != source_port) {
            continue;
        }

        if(unlikely(packet_info.packet_length > *buffer_size)) {
            fprintf(stderr, "Data received is larger than buffer size!\n");
            exit(EXIT_FAILURE);
        }

        printf("got packet\n");

        switch(packet_info.packet_type) {
            case PACKET_TYPE_REQUEST_INFORMATION:
                {
                    ClientInfo client_info;
                    client_info.destination_port = packet_info.client_info.source_port;
                    client_info.source_port = source_port;
        
                    PacketInfo packet_info_new;
                    packet_info_new.client_info = client_info;
                    packet_info_new.packet_type = PACKET_TYPE_REQUEST_INFORMATION;
                    packet_info_new.packet_length = sizeof(ServerInformation);
                    packet_info_new.packet_id = rand();
        
                    uint8_t send_buffer[sizeof(packet_info) + sizeof(ServerInformation)];
                    
                    ServerInformation server_information;
        
                    server_information.maximum_transmission_unit = maximum_transmission_unit;
        
                    memcpy(send_buffer, &packet_info_new, sizeof(packet_info_new));
                    memcpy(&send_buffer[sizeof(packet_info_new)], &server_information, sizeof(server_information));
        
                    sendto(sockfd, send_buffer, sizeof(send_buffer), 0, (struct sockaddr *)&client_address, client_address_len);
        
                    goto next_packet;
                }
            case PACKET_TYPE_SEND_NEXT_CHUNK:
                {
                    PacketSending* target_packet_sending = GetPacketSending(packet_sending, packet_sending_size, packet_info.packet_id);
                    if(unlikely(target_packet_sending == NULL)) {
                        fprintf(stderr, "Got message to send an unknown packet\n");
                        goto next_packet;
                    }

                    target_packet_sending->requested_next_chunk = true;

                    goto next_packet;
                }
        }

        client_address.sin_port = packet_info.client_info.source_port;

        ClientAddrData sender;
        sender.clientAddr = client_address;
        sender.clientAddrLen = client_address_len;
        sender.maximum_transmission_unit = packet_info.chunk_size;

        unsigned int mtu = MIN(packet_info.chunk_size, maximum_transmission_unit);
        const unsigned int chunk_data_size = mtu - sizeof(PacketInfo);
        
        SwiftNetServerCode(
            TransferClient* transfer_client = GetTransferClient(packet_info, client_address.sin_addr.s_addr, server);

            if(transfer_client == NULL) {
                if(packet_info.packet_length + header_size > mtu) {
    
                    TransferClient* newlyCreatedTransferClient = CreateNewTransferClient(packet_info, client_address.sin_addr.s_addr, server);
    
                    // Copy the data from buffer to the transfer client
                    memcpy(newlyCreatedTransferClient->packetCurrentPointer, &packet_buffer[header_size], chunk_data_size);
    
                    newlyCreatedTransferClient->packetCurrentPointer += chunk_data_size;
    
                    RequestNextChunk(server, packet_info.packet_id, sender);
                } else {
                    ClientAddrData sender;
        
                    sender.clientAddr = client_address;
                    sender.clientAddrLen = client_address_len;
    
                    server->packet.packetDataLen = packet_info.packet_length;
    
                    // Execute function set by user
                    server->packetHandler(packet_buffer + header_size, sender);
                }
            } else {
                // Found a transfer client
                unsigned int bytes_needed_to_complete_packet = packet_info.packet_length - (transfer_client->packetCurrentPointer - transfer_client->packetDataStart);

                printf("percentage transmitted: %f\n", 1 - ((float)bytes_needed_to_complete_packet / packet_info.packet_length));
    
                // If this chunk is the last to complpete the packet
                if(bytes_needed_to_complete_packet < mtu) {
                    memcpy(transfer_client->packetCurrentPointer, &packet_buffer[header_size], bytes_needed_to_complete_packet);
    
                    server->packetHandler(transfer_client->packetDataStart, sender);
    
                    free(transfer_client->packetDataStart);
    
                    memset(transfer_client, 0x00, sizeof(TransferClient));
                } else {
                    memcpy(transfer_client->packetCurrentPointer, &packet_buffer[header_size], chunk_data_size);
    
                    transfer_client->packetCurrentPointer += chunk_data_size;
    
                    RequestNextChunk(server, packet_info.packet_id, sender);
                }
            }
        )

        SwiftNetClientCode(
            PendingMessage* pending_message = GetPendingMessage(&packet_info, connection->pending_messages, MAX_PENDING_MESSAGES);

            if(pending_message == NULL) {
                if(packet_info.packet_length + header_size > mtu) {
                    // Split packet into chunks
                    PendingMessage* new_pending_message = CreateNewPendingMessage(connection->pending_messages, MAX_PENDING_MESSAGES, &packet_info);
                    
                    memcpy(new_pending_message->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);

                    RequestNextChunk(connection, packet_info.packet_id);
                } else {
                    connection->packetHandler(packet_buffer);

                    continue;
                }
            } else {
                unsigned int bytes_needed_to_complete_packet = packet_info.packet_length - (pending_message->packet_current_pointer - pending_message->packet_data_start);

                if(bytes_needed_to_complete_packet < mtu) {
                    // Completed the packet
                    memcpy(pending_message->packet_current_pointer, &packet_buffer[header_size], bytes_needed_to_complete_packet);

                    connection->packetHandler(pending_message->packet_data_start);

                    free(pending_message->packet_data_start);

                    memset(pending_message, 0x00, sizeof(PendingMessage));
                } else {
                    memcpy(pending_message->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);

                    pending_message->packet_current_pointer += chunk_data_size;

                    RequestNextChunk(connection, packet_info.packet_id);
                }
            }
        )

        next_packet:
        
        continue;
    }

    return NULL;
}
/*
SwiftNetClientCode(
void* SwiftNetHandlePackets(void* clientVoid) {
    SwiftNetClientConnection* client = (SwiftNetClientConnection*)clientVoid;
    
    const unsigned int headerSize = sizeof(PacketInfo) + sizeof(struct ip);
    const unsigned int totalBufferSize = headerSize + client->dataChunkSize;

    uint8_t packetBuffer[totalBufferSize];


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
*/
