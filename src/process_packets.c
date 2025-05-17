#include "internal/internal.h"
#include "swift_net.h"
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

static inline void packet_completed(const uint16_t packet_id, const unsigned int packet_length, CONNECTION_TYPE* restrict const connection) {
    const SwiftNetPacketCompleted null_packet_completed = {0x00};

    for(uint16_t i = 0; i < MAX_COMPLETED_PACKETS_HISTORY_SIZE; i++) {
        if(memcmp((const void*)&connection->packets_completed_history[i], &null_packet_completed, sizeof(SwiftNetPacketCompleted)) == 0) {
            connection->packets_completed_history[i] = (SwiftNetPacketCompleted){
                .packet_id = packet_id,
                .packet_length = packet_length
            };

            return;
        }
    }
}

static inline bool check_packet_already_completed(const uint16_t packet_id, const CONNECTION_TYPE* restrict const connection) {
    for(uint16_t i = 0; i < MAX_COMPLETED_PACKETS_HISTORY_SIZE; i++) {
        if(connection->packets_completed_history[i].packet_id == packet_id) {
            return true; 
        }
    }

    return false;
}

static inline SwiftNetPendingMessage* const restrict get_pending_message(const SwiftNetPacketInfo* const restrict packet_info, SwiftNetPendingMessage* const restrict pending_messages, const uint16_t pending_messages_size EXTRA_SERVER_ARG(const in_addr_t client_address)) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        SwiftNetPendingMessage* const restrict current_pending_message = &pending_messages[i];

        SwiftNetClientCode(
            const bool target_message = current_pending_message->packet_info.packet_id == packet_info->packet_id;
        )

        SwiftNetServerCode(
            const bool target_message = (current_pending_message->packet_info.packet_id == packet_info->packet_id) && (current_pending_message->client_address == client_address);
        )

        if(target_message == true) {
            return current_pending_message;
        }
    }

    return NULL;
}


static inline void chunk_received(SwiftNetPendingMessage* const restrict pending_message, const unsigned int index) {
    const unsigned int byte = index / 8;
    const uint8_t bit = index % 8;

    pending_message->chunks_received[byte] |= 1 << bit;
}

static inline SwiftNetPendingMessage* restrict const create_new_pending_message(SwiftNetPendingMessage* restrict const pending_messages, const uint16_t pending_messages_size, const SwiftNetPacketInfo* const restrict packet_info EXTRA_SERVER_ARG(const in_addr_t client_address)) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        SwiftNetPendingMessage* restrict const current_pending_message = &pending_messages[i];

        if(current_pending_message->packet_current_pointer == NULL) {
            current_pending_message->packet_info = *packet_info;

            uint8_t* restrict const allocated_memory = malloc(packet_info->packet_length);

            current_pending_message->packet_data_start = allocated_memory;
            current_pending_message->packet_current_pointer = allocated_memory;

            const unsigned int chunks_received_byte_size = (packet_info->chunk_amount + 7) / 8;

            current_pending_message->chunks_received_length = chunks_received_byte_size;
            current_pending_message->chunks_received = calloc(chunks_received_byte_size, 1);

            SwiftNetServerCode(
                current_pending_message->client_address = client_address;
            )

            return current_pending_message;
        }
    }

    return NULL;
}

static inline volatile SwiftNetPacketSending* const get_packet_sending(volatile SwiftNetPacketSending* const packet_sending_array, const uint16_t size, const uint16_t target_id) {
    for(uint16_t i = 0; i < size; i++) {
        volatile SwiftNetPacketSending* const current_packet_sending = &packet_sending_array[i];
        if(current_packet_sending->packet_id == target_id) {
            return current_packet_sending;
        }
    }

    return NULL;
}

PacketQueueNode* wait_for_next_packet() {
    while(packet_queue.first_node == NULL) {
    };

    PacketQueueNode* const restrict node_to_process = packet_queue.first_node;

    if(node_to_process == packet_queue.last_node) {
        packet_queue.first_node = NULL;
        packet_queue.last_node = NULL;

        return node_to_process;
    }

    packet_queue.first_node = node_to_process->next;

    return node_to_process;
}

void* process_packets(void* restrict const void_connection) {
    const unsigned int header_size = sizeof(SwiftNetPacketInfo) + sizeof(struct ip);

    SwiftNetServerCode(
        SwiftNetServer* restrict const server = (SwiftNetServer*)void_connection;

        void (* const volatile * const packet_handler) (uint8_t*, const SwiftNetPacketMetadata) = &server->packet_handler;

        const unsigned min_mtu = maximum_transmission_unit;
        const int sockfd = server->sockfd;
        const uint16_t source_port = server->server_port;
        const unsigned int* const buffer_size = &server->buffer_size;
        volatile SwiftNetPacketSending* const packet_sending = server->packets_sending;
        const uint16_t packet_sending_size = MAX_PACKETS_SENDING;
        uint8_t* current_read_pointer = server->current_read_pointer;

        SwiftNetPendingMessage* restrict const pending_messages = server->pending_messages;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* const restrict connection = (SwiftNetClientConnection*)void_connection;

        void (* const volatile * const packet_handler) (uint8_t*, const SwiftNetPacketMetadata) = &connection->packet_handler;

        const unsigned int min_mtu = MIN(maximum_transmission_unit, connection->maximum_transmission_unit);
        const int sockfd = connection->sockfd;
        const uint16_t source_port = connection->port_info.source_port;
        const volatile unsigned int* const buffer_size = &connection->buffer_size;
        volatile SwiftNetPacketSending* const packet_sending = connection->packets_sending;
        const uint16_t packet_sending_size = MAX_PACKETS_SENDING;
        uint8_t* current_read_pointer = connection->current_read_pointer;

        SwiftNetPendingMessage* restrict const pending_messages = connection->pending_messages;
    )

    while(1) {
        PacketQueueNode* restrict const node = wait_for_next_packet();

        uint8_t* restrict const packet_buffer = node->data;
        if(unlikely(node->data == NULL)) {
            goto next_packet;
        }

        const uint8_t* packet_data = &packet_buffer[header_size];

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
                    
                    const SwiftNetServerInformation server_information = {
                        .maximum_transmission_unit = maximum_transmission_unit
                    };
        
                    memcpy(send_buffer, &packet_info_new, sizeof(packet_info_new));
                    memcpy(&send_buffer[sizeof(packet_info_new)], &server_information, sizeof(server_information));
        
                    sendto(sockfd, send_buffer, sizeof(send_buffer), 0, (struct sockaddr *)&node->sender_address, node->sender_address_len);
        
                    goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_REQUEST:
            {
                SwiftNetPortInfo port_info;
                port_info.source_port = packet_info.port_info.destination_port;
                port_info.destination_port = packet_info.port_info.source_port;

                SwiftNetPacketInfo packet_info_new;
                packet_info_new.packet_id = packet_info.packet_id;
                packet_info_new.packet_type = PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE;
                packet_info_new.port_info = port_info;

                const SwiftNetPendingMessage* restrict const pending_message = get_pending_message(&packet_info, pending_messages, MAX_PENDING_MESSAGES EXTRA_SERVER_ARG(node->sender_address.sin_addr.s_addr));
                if(unlikely(pending_message == NULL)) {
                    const bool packet_already_completed = check_packet_already_completed(packet_info.packet_id, void_connection);
                    if(likely(packet_already_completed == true)) {
                        const SwiftNetPacketInfo send_packet_info = {
                            .packet_length = 0x00,
                            .packet_id = packet_info.packet_id,
                            .packet_type = PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET,
                            .port_info = (SwiftNetPortInfo){
                                .destination_port = packet_info.port_info.source_port,
                                .source_port = packet_info.port_info.destination_port
                            }
                        };

                        sendto(sockfd, &send_packet_info, sizeof(SwiftNetPacketInfo), 0x00, (const struct sockaddr *)&node->sender_address, node->sender_address_len);

                        goto next_packet;
                    }

                    goto next_packet;
                }

                packet_info_new.packet_length = pending_message->chunks_received_length;

                const unsigned int buffer_size = sizeof(SwiftNetPacketInfo) + pending_message->chunks_received_length;
                uint8_t send_buffer[buffer_size];

                memcpy(send_buffer, &packet_info_new, sizeof(SwiftNetPacketInfo));
                memcpy(&send_buffer[sizeof(SwiftNetPacketInfo)], pending_message->chunks_received, pending_message->chunks_received_length);
                
                for(uint8_t i = 0; i < pending_message->chunks_received_length; i++) {
                }

                sendto(sockfd, send_buffer, buffer_size, 0, (const struct sockaddr *)&node->sender_address, node->sender_address_len);

                goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE:
            {
                volatile SwiftNetPacketSending* const target_packet_sending = get_packet_sending(packet_sending, packet_sending_size, packet_info.packet_id);
                if(unlikely(target_packet_sending == NULL)) {
                    goto next_packet;
                }

                if(unlikely(target_packet_sending->lost_chunks_bit_array == NULL)) {
                    target_packet_sending->lost_chunks_bit_array = malloc(packet_info.packet_length);
                }

                memcpy((void*)target_packet_sending->lost_chunks_bit_array, packet_data, packet_info.packet_length);

                target_packet_sending->updated_lost_chunks_bit_array = true;

                goto next_packet;
            }
            case PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET:
            {
                volatile SwiftNetPacketSending* const target_packet_sending = (SwiftNetPacketSending* const restrict)get_packet_sending(packet_sending, packet_sending_size, packet_info.packet_id);

                if(unlikely(target_packet_sending == NULL)) {
                    goto next_packet;
                }

                target_packet_sending->successfully_received = true;

                goto next_packet;
            }
        }

        node->sender_address.sin_port = packet_info.port_info.source_port;

        const SwiftNetClientAddrData sender = {
            .client_address = node->sender_address,
            .client_address_length = node->sender_address_len,
            .maximum_transmission_unit = packet_info.chunk_size
        };

        SwiftNetPacketMetadata packet_metadata;
        packet_metadata.data_length = packet_info.packet_length;

        const unsigned int mtu = MIN(packet_info.chunk_size, maximum_transmission_unit);
        const unsigned int chunk_data_size = mtu - sizeof(SwiftNetPacketInfo);
        
        SwiftNetServerCode(
            packet_metadata.sender = sender;
        )

        SwiftNetPendingMessage* const restrict pending_message = get_pending_message(&packet_info, pending_messages, MAX_PENDING_MESSAGES EXTRA_SERVER_ARG(node->sender_address.sin_addr.s_addr));

        if(pending_message == NULL) {
            if(packet_info.packet_length + header_size > mtu) {
                // Split packet into chunks
                SwiftNetClientCode(
                    SwiftNetPendingMessage* restrict const new_pending_message = create_new_pending_message(pending_messages, MAX_PENDING_MESSAGES, &packet_info);
                )
                SwiftNetServerCode(
                    SwiftNetPendingMessage* restrict const new_pending_message = create_new_pending_message(pending_messages, MAX_PENDING_MESSAGES, &packet_info, node->sender_address.sin_addr.s_addr);
                )

                chunk_received(new_pending_message, packet_info.chunk_index);
                    
                memcpy(new_pending_message->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);

                new_pending_message->packet_current_pointer += chunk_data_size;
            } else {
                current_read_pointer = packet_buffer + header_size;

                packet_completed(packet_info.packet_id, packet_info.packet_length, void_connection);

                (*packet_handler)(packet_buffer + header_size, packet_metadata);

                continue;
            }
        } else {
            const unsigned int bytes_needed_to_complete_packet = packet_info.packet_length - (pending_message->packet_current_pointer - pending_message->packet_data_start);

            chunk_received(pending_message, packet_info.chunk_index);

            if(bytes_needed_to_complete_packet < mtu) {
                    // Completed the packet
                memcpy(pending_message->packet_current_pointer, &packet_buffer[header_size], bytes_needed_to_complete_packet);

                current_read_pointer = pending_message->packet_data_start;

                packet_completed(packet_info.packet_id, packet_info.packet_length, void_connection);

                (*packet_handler)(pending_message->packet_data_start, packet_metadata);

                free(pending_message->packet_data_start);
                free(pending_message->chunks_received);

                memset(pending_message, 0x00, sizeof(SwiftNetPendingMessage));
            } else {
                memcpy(pending_message->packet_current_pointer, &packet_buffer[header_size], chunk_data_size);

                pending_message->packet_current_pointer += chunk_data_size;

                chunk_received(pending_message, packet_info.chunk_index);
            }
        }

        goto next_packet;

    next_packet:

        free(node->data);
        free(node);

        continue;
    }

    return NULL;
}
