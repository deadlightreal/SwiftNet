#include "internal/internal.h"
#include "swift_net.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/_types/_in_addr_t.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

// Returns an array of 4 byte uint32_tegers, that contain indexes of lost chunks
static inline const uint32_t return_lost_chunk_indexes(const uint8_t* const restrict chunks_received, const uint32_t chunk_amount, const uint32_t buffer_size, uint32_t* const restrict buffer) {
    uint32_t byte = 0;

    uint32_t offset = 0;

    while(1) {
        if(byte * 8 + 8 < chunk_amount) {
            if(chunks_received[byte] == 0xFF) {
                byte++;
                continue;
            }

            for(uint8_t bit = 0; bit < 8; bit++) {
                if(offset * 4 + 4 > buffer_size) { 
                    return buffer_size;
                }

                if((chunks_received[byte] & (1 << bit)) == 0x00) {
                    buffer[offset] = byte * 8 + bit;
                    offset++;
                }
            }
        } else {
            const uint8_t bits_to_check = chunk_amount - byte * 8;
            
            for(uint8_t bit = 0; bit < bits_to_check; bit++) {
                if(offset * 4 + 4 > buffer_size) { 
                    return buffer_size;
                }
                
                if((chunks_received[byte] & (1 << bit)) == 0x00) {
                    buffer[offset] = byte * 8 + bit;
                    offset++;
                }
            }
            
            return offset * 4;
        }

        byte++;
    }

    return offset * 4;
}

static inline void packet_completed(const uint16_t packet_id, const uint32_t packet_length, volatile SwiftNetPacketCompleted* const packets_completed_history) {
    const SwiftNetPacketCompleted null_packet_completed = {0x00};

    for(uint16_t i = 0; i < MAX_COMPLETED_PACKETS_HISTORY_SIZE; i++) {
        if(memcmp((const void*)&packets_completed_history[i], &null_packet_completed, sizeof(SwiftNetPacketCompleted)) == 0) {
            packets_completed_history[i] = (SwiftNetPacketCompleted){
                .packet_id = packet_id,
                .packet_length = packet_length
            };

            return;
        }
    }
}

static inline bool check_packet_already_completed(const uint16_t packet_id, volatile const SwiftNetPacketCompleted* const packets_completed_history) {
    for(uint16_t i = 0; i < MAX_COMPLETED_PACKETS_HISTORY_SIZE; i++) {
        if(packets_completed_history[i].packet_id == packet_id) {
            return true; 
        }
    }

    return false;
}

static inline SwiftNetPendingMessage* const restrict get_pending_message(const SwiftNetPacketInfo* const restrict packet_info, SwiftNetPendingMessage* const restrict pending_messages, const uint16_t pending_messages_size, const ConnectionType connection_type, const in_addr_t sender_address) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        SwiftNetPendingMessage* const restrict current_pending_message = &pending_messages[i];

        if((connection_type == CONNECTION_TYPE_CLIENT && current_pending_message->packet_info.packet_id == packet_info->packet_id) || (connection_type == CONNECTION_TYPE_SERVER && current_pending_message->sender_address == sender_address)) {
            return current_pending_message;
        }
    }

    return NULL;
}


static inline void chunk_received(uint8_t* const restrict chunks_received, const uint32_t index) {
    const uint32_t byte = index / 8;
    const uint8_t bit = index % 8;

    chunks_received[byte] |= 1 << bit;
}

static inline SwiftNetPendingMessage* restrict const create_new_pending_message(SwiftNetPendingMessage* restrict const pending_messages, const uint16_t pending_messages_size, const SwiftNetPacketInfo* const restrict packet_info, const ConnectionType connection_type, in_addr_t sender_address) {
    for(uint16_t i = 0; i < pending_messages_size; i++) {
        SwiftNetPendingMessage* restrict const current_pending_message = &pending_messages[i];

        if(current_pending_message->packet_data_start == NULL) {
            uint8_t* restrict const allocated_memory = malloc(packet_info->packet_length);

            const uint32_t chunks_received_byte_size = (packet_info->chunk_amount + 7) / 8;

            current_pending_message->packet_info = *packet_info;

            current_pending_message->packet_data_start = allocated_memory;
            current_pending_message->chunks_received_number = 0x00;

            current_pending_message->chunks_received_length = chunks_received_byte_size;
            current_pending_message->chunks_received = calloc(chunks_received_byte_size, 1);

            if(connection_type == CONNECTION_TYPE_SERVER) {
                current_pending_message->sender_address = sender_address;
            }

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

PacketQueueNode* wait_for_next_packet(PacketQueue* restrict const packet_queue) {
    uint32_t owner_none = PACKET_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong(&packet_queue->owner, &owner_none, PACKET_QUEUE_OWNER_PROCESS_PACKETS)) {
        owner_none = PACKET_QUEUE_OWNER_NONE;
    }

    if(packet_queue->first_node == NULL) {
        atomic_store(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE);
        return NULL;
    }

    PacketQueueNode* volatile const node_to_process = packet_queue->first_node;

    if(node_to_process->next == NULL) {
        packet_queue->first_node = NULL;
        packet_queue->last_node = NULL;

        atomic_store(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE);

        return node_to_process;
    }

    packet_queue->first_node = node_to_process->next;

    atomic_store(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE);

    return node_to_process;
}

static inline bool packet_corrupted(const uint32_t checksum, const uint32_t chunk_size, const uint8_t* restrict const buffer) {
    return crc32(buffer, chunk_size) != checksum;
}

static inline void swiftnet_process_packets(
    void (* const volatile * const packet_handler) (uint8_t*, void*),
    const int sockfd,
    const uint16_t source_port,
    volatile const uint32_t* const buffer_size,
    volatile SwiftNetPacketSending* const packet_sending,
    SwiftNetPendingMessage* restrict const pending_messages,
    const uint16_t packet_sending_size,
    uint8_t* current_read_pointer,
    volatile SwiftNetPacketCompleted* const packets_completed_history,
    ConnectionType connection_type,
    PacketQueue* restrict const packet_queue
) {
    while(1) {
        PacketQueueNode* restrict const node = wait_for_next_packet(packet_queue);
        if(node == NULL) {
            continue;
        }

        uint8_t* restrict const packet_buffer = node->data;
        if(packet_buffer == NULL) {
            goto next_packet;
        }

        uint8_t* restrict const packet_data = &packet_buffer[PACKET_HEADER_SIZE];

        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetErrorCheck(
            if(unlikely(*packet_handler == NULL)) {
                fprintf(stderr, "Message Handler not set!!\n");
                continue;
            }
        )

        struct ip ip_header;
        memcpy(&ip_header, packet_buffer, sizeof(ip_header));

        SwiftNetPacketInfo packet_info;
        memcpy(&packet_info, &packet_buffer[sizeof(ip_header)], sizeof(packet_info));

        // Check if the packet is meant to be for this server
        if(packet_info.port_info.destination_port != source_port) {
            goto next_packet;
        }

        const uint32_t checksum_received = packet_info.checksum;

        memset(&packet_buffer[sizeof(struct ip) + offsetof(SwiftNetPacketInfo, checksum)], 0x00, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));

        if(packet_corrupted(checksum_received, packet_info.chunk_size + sizeof(SwiftNetPacketInfo), &packet_buffer[sizeof(struct ip)]) == true) {
            goto next_packet;
        }

        if(unlikely(packet_info.packet_length > *buffer_size)) {
            fprintf(stderr, "Data received is larger than buffer size!\n");
            exit(EXIT_FAILURE);
        }

        switch(packet_info.packet_type) {
            case PACKET_TYPE_REQUEST_INFORMATION:
            {
                    const SwiftNetPacketInfo packet_info_new = {
                        .port_info = (SwiftNetPortInfo){
                            .source_port = source_port,
                            .destination_port = packet_info.port_info.source_port
                        },
                        .packet_type = PACKET_TYPE_REQUEST_INFORMATION,
                        .packet_length = sizeof(SwiftNetServerInformation),
                        .chunk_size = sizeof(SwiftNetServerInformation),
                        .chunk_amount = 1,
                        .packet_id = rand(),
                        .checksum = 0x00,
                        .maximum_transmission_unit = maximum_transmission_unit
                    };

                    const SwiftNetServerInformation server_information = {
                        .maximum_transmission_unit = maximum_transmission_unit
                    };

                    uint8_t send_buffer[sizeof(packet_info_new) + sizeof(SwiftNetServerInformation)];
        
                    memcpy(send_buffer, &packet_info_new, sizeof(packet_info_new));
                    memcpy(&send_buffer[sizeof(packet_info_new)], &server_information, sizeof(server_information));

                    const uint32_t checksum = crc32(send_buffer, sizeof(send_buffer));

                    memcpy(&send_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));

                    sendto(sockfd, send_buffer, sizeof(send_buffer), 0, (struct sockaddr *)&node->sender_address, node->server_address_length);
        
                    goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_REQUEST:
            {
                const uint32_t mtu = MIN(packet_info.maximum_transmission_unit, maximum_transmission_unit);

                SwiftNetPacketInfo packet_info_new = {
                    .port_info = (SwiftNetPortInfo){
                        .destination_port = packet_info.port_info.source_port,
                        .source_port = packet_info.port_info.destination_port
                    },
                    .checksum = 0x00,
                    .packet_id = packet_info.packet_id,
                    .packet_type = PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE,
                    .chunk_size = maximum_transmission_unit - PACKET_HEADER_SIZE,
                    .maximum_transmission_unit = maximum_transmission_unit
                };

                SwiftNetPendingMessage* restrict const pending_message = get_pending_message(&packet_info, pending_messages, MAX_PENDING_MESSAGES, connection_type, ip_header.ip_src.s_addr);
                if(pending_message == NULL) {
                    const bool packet_already_completed = check_packet_already_completed(packet_info.packet_id, packets_completed_history);
                    if(likely(packet_already_completed == true)) {
                        SwiftNetPacketInfo send_packet_info = {
                            .packet_length = 0x00,
                            .chunk_size = 0x00,
                            .chunk_amount = 1,
                            .packet_id = packet_info.packet_id,
                            .packet_type = PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET,
                            .port_info = (SwiftNetPortInfo){
                                .destination_port = packet_info.port_info.source_port,
                                .source_port = packet_info.port_info.destination_port
                            },
                            .checksum = 0x00,
                            .maximum_transmission_unit = maximum_transmission_unit
                        };

                        send_packet_info.checksum = crc32((uint8_t*)&send_packet_info, sizeof(send_packet_info));

                        sendto(sockfd, &send_packet_info, sizeof(SwiftNetPacketInfo), 0x00, (const struct sockaddr *)&node->sender_address, node->server_address_length);

                        goto next_packet;
                    }

                    goto next_packet;
                }

                uint8_t send_buffer[mtu - sizeof(struct ip)];
                const uint32_t lost_chunk_indexes = return_lost_chunk_indexes(pending_message->chunks_received, packet_info.chunk_amount, mtu - PACKET_HEADER_SIZE, (uint32_t*)&send_buffer[sizeof(SwiftNetPacketInfo)]);
                for(uint32_t i = 0; i < lost_chunk_indexes; i += 4) {
                    // Cast to uint32_t pointer before dereferencing to ensure correct size
                    uint32_t *packet = (uint32_t*)&send_buffer[sizeof(SwiftNetPacketInfo) + i];
                }

                packet_info_new.packet_length = lost_chunk_indexes;
                packet_info_new.chunk_size = lost_chunk_indexes;

                memcpy(send_buffer, &packet_info_new, sizeof(SwiftNetPacketInfo));

                const uint32_t checksum = crc32(send_buffer, sizeof(SwiftNetPacketInfo) + lost_chunk_indexes);

                memcpy(&send_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));

                sendto(sockfd, send_buffer, sizeof(SwiftNetPacketInfo) + lost_chunk_indexes, 0x00, (const struct sockaddr *)&node->sender_address, node->server_address_length);

                goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE:
            {
                volatile SwiftNetPacketSending* const target_packet_sending = get_packet_sending(packet_sending, packet_sending_size, packet_info.packet_id);

                if(unlikely(target_packet_sending == NULL)) {
                    goto next_packet;
                }

                if(unlikely(target_packet_sending->lost_chunks == NULL)) {
                    target_packet_sending->lost_chunks = malloc(maximum_transmission_unit - PACKET_HEADER_SIZE);
                }

                memcpy((void*)target_packet_sending->lost_chunks, packet_data, packet_info.packet_length);

                target_packet_sending->lost_chunks_size = packet_info.packet_length / 4;

                target_packet_sending->updated_lost_chunks = true;

                goto next_packet;
            }
            case PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET:
            {
                volatile SwiftNetPacketSending* const target_packet_sending = get_packet_sending(packet_sending, packet_sending_size, packet_info.packet_id);

                if(unlikely(target_packet_sending == NULL)) {
                    goto next_packet;
                }

                target_packet_sending->successfully_received = true;

                goto next_packet;
            }
        }

        node->sender_address.sin_port = packet_info.port_info.source_port;

        const SwiftNetClientAddrData sender = {
            .sender_address = node->sender_address,
            .maximum_transmission_unit = packet_info.chunk_size
        };

        const uint32_t mtu = MIN(packet_info.maximum_transmission_unit, maximum_transmission_unit);
        const uint32_t chunk_data_size = mtu - PACKET_HEADER_SIZE;

        SwiftNetPendingMessage* const restrict pending_message = get_pending_message(&packet_info, pending_messages, MAX_PENDING_MESSAGES, connection_type, node->sender_address.sin_addr.s_addr);

        if(pending_message == NULL) {
            if(packet_info.packet_length + PACKET_HEADER_SIZE > mtu) {
                // Split packet into chunks
                SwiftNetPendingMessage* restrict const new_pending_message = create_new_pending_message(pending_messages, MAX_PENDING_MESSAGES, &packet_info, connection_type, node->sender_address.sin_addr.s_addr);

                new_pending_message->chunks_received_number++;

                chunk_received(new_pending_message->chunks_received, packet_info.chunk_index);
                    
                memcpy(new_pending_message->packet_data_start, &packet_buffer[PACKET_HEADER_SIZE], chunk_data_size);
            } else {
                current_read_pointer = packet_buffer + PACKET_HEADER_SIZE;

                packet_completed(packet_info.packet_id, packet_info.packet_length, packets_completed_history);

                if(connection_type == CONNECTION_TYPE_SERVER) {
                    SwiftNetPacketServerMetadata packet_metadata = {
                        .data_length = packet_info.packet_length,
                        .sender = sender
                    };

                    (*packet_handler)(packet_buffer + PACKET_HEADER_SIZE, &packet_metadata);
                } else {
                    SwiftNetPacketClientMetadata packet_metadata = {
                        .data_length = packet_info.packet_length,
                    };

                    (*packet_handler)(packet_buffer + PACKET_HEADER_SIZE, &packet_metadata);
                }

                continue;
            }
        } else {
            const uint32_t bytes_to_copy = packet_info.chunk_amount == packet_info.chunk_index + 1 ? packet_info.packet_length % chunk_data_size : chunk_data_size;

            if(pending_message->chunks_received_number + 1 >= packet_info.chunk_amount) {
                    // Completed the packet
                memcpy(pending_message->packet_data_start + (chunk_data_size * packet_info.chunk_index), &packet_buffer[PACKET_HEADER_SIZE], bytes_to_copy);

                current_read_pointer = pending_message->packet_data_start;

                packet_completed(packet_info.packet_id, packet_info.packet_length, packets_completed_history);

                if(connection_type == CONNECTION_TYPE_SERVER) {
                    SwiftNetPacketServerMetadata packet_metadata = {
                        .data_length = packet_info.packet_length,
                        .sender = sender
                    };

                    (*packet_handler)(packet_buffer + PACKET_HEADER_SIZE, &packet_metadata);
                } else {
                    SwiftNetPacketClientMetadata packet_metadata = {
                        .data_length = packet_info.packet_length,
                    };

                    (*packet_handler)(packet_buffer + PACKET_HEADER_SIZE, &packet_metadata);
                }

                free(pending_message->packet_data_start);
                free(pending_message->chunks_received);

                memset(pending_message, 0x00, sizeof(SwiftNetPendingMessage));
            } else {
                memcpy(pending_message->packet_data_start + (chunk_data_size * packet_info.chunk_index), &packet_buffer[PACKET_HEADER_SIZE], bytes_to_copy);

                chunk_received(pending_message->chunks_received, packet_info.chunk_index);

                pending_message->chunks_received_number++;
            }
        }

        goto next_packet;

    next_packet:
        free(node->data);
        free(node);

        continue;
    }
}

void* swiftnet_server_process_packets(void* restrict const void_server) {
    SwiftNetServer* restrict const server = (SwiftNetServer*)void_server;

    swiftnet_process_packets((void (* const volatile * const) (uint8_t*, void*))&server->packet_handler, server->sockfd, server->server_port, &server->buffer_size, server->packets_sending, server->pending_messages, MAX_PACKETS_SENDING, server->current_read_pointer, server->packets_completed_history, CONNECTION_TYPE_SERVER, &server->packet_queue);

    return NULL;
}

void* swiftnet_client_process_packets(void* restrict const void_client) {
    SwiftNetClientConnection* restrict const client = (SwiftNetClientConnection*)void_client;

    swiftnet_process_packets((void (* const volatile * const) (uint8_t*, void*))&client->packet_handler, client->sockfd, client->port_info.source_port, &client->buffer_size, client->packets_sending, client->pending_messages, MAX_PACKETS_SENDING, client->current_read_pointer, client->packets_completed_history, CONNECTION_TYPE_CLIENT, &client->packet_queue);

    return NULL;
}
