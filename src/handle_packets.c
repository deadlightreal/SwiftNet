#include "swift_net.h"
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include "internal/internal.h"

SwiftNetServerCode(
static inline SwiftNetTransferClient* get_transfer_client(SwiftNetPacketInfo packet_info, in_addr_t client_address, SwiftNetServer* server) {
    for(unsigned int i = 0; i < MAX_TRANSFER_CLIENTS; i++) {
        SwiftNetTransferClient* current_transfer_client = &server->transfer_clients[i];

        if(current_transfer_client->packet_info.port_info.source_port == packet_info.port_info.source_port && client_address == current_transfer_client->client_address && packet_info.packet_id == current_transfer_client->packet_info.packet_id) {
            // Found a transfer client struct that is meant for this client
            return current_transfer_client;
        }
    }

    return NULL;
}

static inline SwiftNetTransferClient* create_new_transfer_client(SwiftNetPacketInfo packet_info, in_addr_t client_address, SwiftNetServer* server) {
    SwiftNetTransferClient empty_transfer_client = {0x00};

    for(unsigned int i = 0; i < MAX_TRANSFER_CLIENTS; i++) {
        SwiftNetTransferClient* current_transfer_client = &server->transfer_clients[i];

        if(memcmp(&empty_transfer_client, current_transfer_client, sizeof(SwiftNetTransferClient)) == 0) {
            // Found empty transfer client slot
            current_transfer_client->client_address = client_address;
            current_transfer_client->packet_info = packet_info;
            
            uint8_t* allocated_memory = malloc(packet_info.packet_length);
            
            current_transfer_client->packet_data_start = allocated_memory;
            current_transfer_client->packet_current_pointer = allocated_memory;

            return current_transfer_client;
        }
    }

    fprintf(stderr, "Error: exceeded maximum number of transfer clients\n");
    exit(EXIT_FAILURE);
}
)

SwiftNetClientCode(
static inline PendingMessage* get_pending_message(SwiftNetPacketInfo* packet_info, PendingMessage* pending_messages, uint16_t pending_messages_size) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        PendingMessage* current_pending_message = &pending_messages[i];

        if(current_pending_message->packet_info.packet_id == packet_info->packet_id && current_pending_message->packet_info.packet_length == packet_info->packet_length) {
            return current_pending_message;
        }
    }

    return NULL;
}

static inline PendingMessage* create_new_pending_message(PendingMessage* pending_messages, uint16_t pending_messages_size, SwiftNetPacketInfo* packet_info) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        PendingMessage* current_pending_message = &pending_messages[i];

        if(current_pending_message->packet_current_pointer == NULL) {
            current_pending_message->packet_info = *packet_info;

            uint8_t* allocated_memory = malloc(packet_info->packet_length);

            current_pending_message->packet_data_start = allocated_memory;
            current_pending_message->packet_current_pointer = allocated_memory;

            return current_pending_message;
        }
    }

    return NULL;
}
)

static inline void request_next_chunk(CONNECTION_TYPE* connection, uint16_t packet_id EXTRA_REQUEST_NEXT_CHUNK_ARG) {
    SwiftNetPacketInfo packet_info;
    packet_info.packet_id = packet_id;
    packet_info.packet_length = 0;
    packet_info.packet_type = PACKET_TYPE_SEND_NEXT_CHUNK;

    SwiftNetServerCode(
        const uint16_t source_port = connection->server_port;
        const uint16_t destination_port = target.client_address.sin_port;
        const int sockfd = connection->sockfd;
        const struct sockaddr_in* target_sock_addr = &target.client_address;
        const socklen_t socklen = target.client_address_length;
    )

    SwiftNetClientCode(
        const uint16_t source_port = connection->port_info.source_port;
        const uint16_t destination_port = connection->port_info.destination_port;
        const int sockfd = connection->sockfd;
        const struct sockaddr_in* target_sock_addr = &connection->server_addr;
        const socklen_t socklen = sizeof(connection->server_addr);
    )

    packet_info.port_info.source_port = source_port;
    packet_info.port_info.destination_port = destination_port;

    sendto(sockfd, &packet_info, sizeof(SwiftNetPacketInfo), 0, (const struct sockaddr *)target_sock_addr, socklen);
}

static inline SwiftNetPacketSending* get_packet_sending(SwiftNetPacketSending* packet_sending_array, const uint16_t size, const uint16_t target_id) {
    for(uint16_t i = 0; i < size; i++) {
        SwiftNetPacketSending* current_packet_sending = &packet_sending_array[i];
        if(current_packet_sending->packet_id == target_id) {
            return current_packet_sending;
        }
    }

    return NULL;
}

void* swiftnet_handle_packets(void* void_connection) {
    const unsigned int header_size = sizeof(SwiftNetPacketInfo) + sizeof(struct ip);

    SwiftNetServerCode(
        SwiftNetServer* server = (SwiftNetServer*)void_connection;

        void** packet_handler = (void**)&server->packet_handler;

        const unsigned min_mtu = maximum_transmission_unit;
        const int sockfd = server->sockfd;
        const uint16_t source_port = server->server_port;
        const unsigned int* buffer_size = &server->buffer_size;
        SwiftNetPacketSending* packet_sending = server->packets_sending;
        const uint16_t packet_sending_size = MAX_PACKETS_SENDING;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* connection = (SwiftNetClientConnection*)void_connection;

        void** packet_handler = (void**)&connection->packet_handler;

        unsigned int min_mtu = MIN(maximum_transmission_unit, connection->maximum_transmission_unit);
        const int sockfd = connection->sockfd;
        const uint16_t source_port = connection->port_info.source_port;
        const unsigned int* buffer_size = &connection->buffer_size;
        SwiftNetPacketSending* packet_sending = connection->packets_sending;
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
            if(unlikely(*packet_handler == NULL)) {
                fprintf(stderr, "Message Handler not set!!\n");
                exit(EXIT_FAILURE);;
            }
        )

        struct ip ip_header;
        memcpy(&ip_header, packet_buffer, sizeof(ip_header));

        SwiftNetPacketInfo packet_info;
        memcpy(&packet_info, &packet_buffer[sizeof(ip_header)], sizeof(SwiftNetPacketInfo));

        // Check if the packet is meant to be for this server
        if(packet_info.port_info.destination_port != source_port) {
            continue;
        }

        if(unlikely(packet_info.packet_length > *buffer_size)) {
            fprintf(stderr, "Data received is larger than buffer size!\n");
            exit(EXIT_FAILURE);
        }

        switch(packet_info.packet_type) {
            case PACKET_TYPE_REQUEST_INFORMATION:
                {
                    SwiftNetPortInfo port_info;
                    port_info.destination_port = packet_info.port_info.source_port;
                    port_info.source_port = source_port;
        
                    SwiftNetPacketInfo packet_info_new;
                    packet_info_new.port_info = port_info;
                    packet_info_new.packet_type = PACKET_TYPE_REQUEST_INFORMATION;
                    packet_info_new.packet_length = sizeof(SwiftNetServerInformation);
                    packet_info_new.packet_id = rand();
        
                    uint8_t send_buffer[sizeof(packet_info) + sizeof(SwiftNetServerInformation)];
                    
                    SwiftNetServerInformation server_information;
        
                    server_information.maximum_transmission_unit = maximum_transmission_unit;
        
                    memcpy(send_buffer, &packet_info_new, sizeof(packet_info_new));
                    memcpy(&send_buffer[sizeof(packet_info_new)], &server_information, sizeof(server_information));
        
                    sendto(sockfd, send_buffer, sizeof(send_buffer), 0, (struct sockaddr *)&client_address, client_address_len);
        
                    goto next_packet;
                }
            case PACKET_TYPE_SEND_NEXT_CHUNK:
                {
                    SwiftNetPacketSending* target_packet_sending = get_packet_sending(packet_sending, packet_sending_size, packet_info.packet_id);
                    if(unlikely(target_packet_sending == NULL)) {
                        fprintf(stderr, "Got message to send an unknown packet\n");
                        goto next_packet;
                    }

                    target_packet_sending->requested_next_chunk = true;

                    goto next_packet;
                }
        }

        client_address.sin_port = packet_info.port_info.source_port;

        SwiftNetClientAddrData sender;
        sender.client_address = client_address;
        sender.client_address_length = client_address_len;
        sender.maximum_transmission_unit = packet_info.chunk_size;

        unsigned int mtu = MIN(packet_info.chunk_size, maximum_transmission_unit);
        const unsigned int chunk_data_size = mtu - sizeof(SwiftNetPacketInfo);
        
        SwiftNetServerCode(
            SwiftNetTransferClient* transfer_client = get_transfer_client(packet_info, client_address.sin_addr.s_addr, server);

            if(transfer_client == NULL) {
                if(packet_info.packet_length + header_size > mtu) {
                    SwiftNetTransferClient* newlyCreatedTransferClient = create_new_transfer_client(packet_info, client_address.sin_addr.s_addr, server);
    
                    // Copy the data from buffer to the transfer client
                    memcpy(newlyCreatedTransferClient->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);
    
                    newlyCreatedTransferClient->packet_current_pointer += chunk_data_size;
    
                    request_next_chunk(server, packet_info.packet_id, sender);
                } else {
                    SwiftNetClientAddrData sender;
        
                    sender.client_address = client_address;
                    sender.client_address_length = client_address_len;
                    sender.maximum_transmission_unit = packet_info.chunk_size;

                    server->current_read_pointer = packet_buffer + header_size;

                    // Execute function set by user
                    server->packet_handler(packet_buffer + header_size, sender);
                }
            } else {
                // Found a transfer client
                unsigned int bytes_needed_to_complete_packet = packet_info.packet_length - (transfer_client->packet_current_pointer - transfer_client->packet_data_start);

                printf("percentage transmitted: %f\n", 1 - ((float)bytes_needed_to_complete_packet / packet_info.packet_length));

                // If this chunk is the last to complpete the packet
                if(bytes_needed_to_complete_packet < mtu) {
                    memcpy(transfer_client->packet_current_pointer, &packet_buffer[header_size], bytes_needed_to_complete_packet);

                    server->current_read_pointer = transfer_client->packet_data_start;
    
                    server->packet_handler(transfer_client->packet_data_start, sender);
    
                    free(transfer_client->packet_data_start);
    
                    memset(transfer_client, 0x00, sizeof(SwiftNetTransferClient));
                } else {
                    memcpy(transfer_client->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);
    
                    transfer_client->packet_current_pointer += chunk_data_size;

                    request_next_chunk(server, packet_info.packet_id, sender);
                }
            }
        )

        SwiftNetClientCode(
            PendingMessage* pending_message = get_pending_message(&packet_info, connection->pending_messages, MAX_PENDING_MESSAGES);

            if(pending_message == NULL) {
                if(packet_info.packet_length + header_size > mtu) {
                    // Split packet into chunks
                    PendingMessage* new_pending_message = create_new_pending_message(connection->pending_messages, MAX_PENDING_MESSAGES, &packet_info);
                    
                    memcpy(new_pending_message->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);

                    request_next_chunk(connection, packet_info.packet_id);
                } else {
                    connection->current_read_pointer = packet_buffer + header_size;

                    connection->packet_handler(packet_buffer + header_size);

                    continue;
                }
            } else {
                unsigned int bytes_needed_to_complete_packet = packet_info.packet_length - (pending_message->packet_current_pointer - pending_message->packet_data_start);

                if(bytes_needed_to_complete_packet < mtu) {
                    // Completed the packet
                    memcpy(pending_message->packet_current_pointer, &packet_buffer[header_size], bytes_needed_to_complete_packet);

                    connection->current_read_pointer = pending_message->packet_data_start;

                    connection->packet_handler(pending_message->packet_data_start);

                    free(pending_message->packet_data_start);

                    memset(pending_message, 0x00, sizeof(PendingMessage));
                } else {
                    memcpy(pending_message->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);

                    pending_message->packet_current_pointer += chunk_data_size;

                    request_next_chunk(connection, packet_info.packet_id);
                }
            }
        )

        next_packet:
        
        continue;
    }

    return NULL;
}
