#include "internal/internal.h"
#include "swift_net.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

static inline const uint32_t return_lost_chunk_indexes(const SwiftNetPendingMessage* const restrict pending_message, const uint32_t buffer_size, uint32_t* const restrict buffer) {
    uint32_t byte = 0;

    uint32_t offset = 0;

    const uint32_t chunk_amount = pending_message->packet_info.chunk_amount;

    while(1) {
        printf("buffer size: %d\noffset: %d\nchunk amount: %d\n", buffer_size, offset, chunk_amount);
        printf("byte: %d\n", byte);

        if(byte * 8 + 8 < pending_message->packet_info.chunk_amount) {
            if(pending_message->chunks_received[byte] == 0xFF) {
                byte++;
                continue;
            }

            for(uint8_t bit = 0; bit < 8; bit++) {
                if(offset * 4 + 4 > buffer_size) { 
                    return buffer_size;
                }

                if((pending_message->chunks_received[byte] & (1 << bit)) == 0x00) {
                    buffer[offset] = byte * 8 + bit;
                    printf("lost: %d\n", buffer[offset]);
                    offset++;
                }
            }
        } else {
            printf("in return stack frame\n");

            const uint8_t bits_to_check = chunk_amount - byte * 8;
            
            for(uint8_t bit = 0; bit < bits_to_check; bit++) {
                if(offset * 4 + 4 > buffer_size) { 
                    return buffer_size;
                }
                
                if((pending_message->chunks_received[byte] & (1 << bit)) == 0x00) {
                    buffer[offset] = byte * 8 + bit;
                    printf("lost: %d\n", buffer[offset]);
                    offset++;
                }
            }
            
            return offset * 4;
        }

        byte++;
    }

    return offset * 4;
}

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

        if(current_pending_message->packet_data_start == NULL) {
            current_pending_message->packet_info = *packet_info;

            uint8_t* restrict const allocated_memory = malloc(packet_info->packet_length);

            current_pending_message->packet_data_start = allocated_memory;
            current_pending_message->chunks_received_number = 0;

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

    unsigned int owner_none = PACKET_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong(&packet_queue.owner, &owner_none, PACKET_QUEUE_OWNER_PROCESS_PACKETS)) {}

    PacketQueueNode* volatile const node_to_process = packet_queue.first_node;

    if(node_to_process->next == NULL) {
        packet_queue.first_node = NULL;
        packet_queue.last_node = NULL;

        return node_to_process;
    }

    packet_queue.first_node = node_to_process->next;

    atomic_store(&packet_queue.owner, PACKET_QUEUE_OWNER_NONE);

    return node_to_process;
}

static inline bool packet_corrupted(uint32_t checksum, uint32_t chunk_size, uint8_t* buffer) {
    printf("hashing %d bytes\n", chunk_size);
    printf("got checksum: %d\nreal checksum: %d\n", checksum, crc32(buffer, chunk_size));
    
    return crc32(buffer, chunk_size) != checksum;
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
        if(packet_buffer == NULL) {
            goto next_packet;
        }

        uint8_t* restrict const packet_data = &packet_buffer[header_size];

        // Check if user set a function that will execute with the packet data received as arg
        SwiftNetErrorCheck(
            if(unlikely(*packet_handler == NULL)) {
                fprintf(stderr, "Message Handler not set!!\n");
                exit(EXIT_FAILURE);;
            }
        )

        printf("got packet\n");

        struct ip ip_header;
        memcpy(&ip_header, packet_buffer, sizeof(ip_header));

        SwiftNetPacketInfo packet_info;
        memcpy(&packet_info, &packet_buffer[sizeof(ip_header)], sizeof(SwiftNetPacketInfo));

        // Check if the packet is meant to be for this server
        if(packet_info.port_info.destination_port != source_port) {
            goto next_packet;
        }

        const uint32_t checksum_received = packet_info.checksum;

        memset(&packet_buffer[sizeof(struct ip) + offsetof(SwiftNetPacketInfo, checksum)], 0x00, sizeof(uint32_t));

        uint32_t packet_data_size = packet_info.chunk_size;

        if(packet_info.chunk_index + 1 == packet_info.chunk_amount) {
            packet_data_size = packet_info.packet_length % (packet_info.chunk_size - sizeof(SwiftNetPacketInfo));
        }

        printf("chunk size: %d\nindex: %d\namount: %d\n", packet_info.chunk_size, packet_info.chunk_index, packet_info.chunk_amount);

        printf("len: %d\n", packet_data_size);

        for (unsigned int i = 0; i < sizeof(SwiftNetPacketInfo); i++) {
            printf("%d ", packet_buffer[sizeof(struct ip) + i]);
        }
        printf("\n");

        if(packet_corrupted(checksum_received, packet_info.chunk_size + sizeof(SwiftNetPacketInfo), &packet_buffer[sizeof(struct ip)]) == true) {
            printf("packet corrupted\n");
            goto next_packet;
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
                    packet_info_new.checksum = 0x00;
                    packet_info_new.maximum_transmission_unit = maximum_transmission_unit;
        
                    uint8_t send_buffer[sizeof(packet_info) + sizeof(SwiftNetServerInformation)];
                    
                    const SwiftNetServerInformation server_information = {
                        .maximum_transmission_unit = maximum_transmission_unit
                    };
        
                    memcpy(send_buffer, &packet_info_new, sizeof(packet_info_new));
                    
                    memcpy(&send_buffer[sizeof(packet_info_new)], &server_information, sizeof(server_information));

                    uint32_t checksum = crc32(send_buffer, sizeof(send_buffer));

                    memcpy(&send_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));
        
                    sendto(sockfd, send_buffer, sizeof(send_buffer), 0, (struct sockaddr *)&node->sender_address, node->sender_address_len);
        
                    goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_REQUEST:
            {
                const unsigned int mtu = MIN(packet_info.maximum_transmission_unit + sizeof(SwiftNetPacketInfo), maximum_transmission_unit);

                SwiftNetPacketInfo packet_info_new = {
                    .port_info = (SwiftNetPortInfo){
                        .destination_port = packet_info.port_info.source_port,
                        .source_port = packet_info.port_info.destination_port
                    },
                    .checksum = 0x00,
                    .packet_id = packet_info.packet_id,
                    .packet_type = PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE,
                    .chunk_size = maximum_transmission_unit - sizeof(SwiftNetPacketInfo),
                    .maximum_transmission_unit = maximum_transmission_unit
                };

                SwiftNetPendingMessage* restrict const pending_message = get_pending_message(&packet_info, pending_messages, MAX_PENDING_MESSAGES EXTRA_SERVER_ARG(node->sender_address.sin_addr.s_addr));
                if(pending_message == NULL) {
                    const bool packet_already_completed = check_packet_already_completed(packet_info.packet_id, void_connection);
                    if(likely(packet_already_completed == true)) {
                        SwiftNetPacketInfo send_packet_info = {
                            .packet_length = 0x00,
                            .chunk_size = 0x00,
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

                        sendto(sockfd, &send_packet_info, sizeof(SwiftNetPacketInfo), 0x00, (const struct sockaddr *)&node->sender_address, node->sender_address_len);

                        goto next_packet;
                    }

                    goto next_packet;
                }

                uint8_t send_buffer[mtu];
                const uint32_t lost_chunk_indexes = return_lost_chunk_indexes(pending_message, mtu - sizeof(SwiftNetPacketInfo), (uint32_t*)&send_buffer[sizeof(SwiftNetPacketInfo)]);
                for(uint32_t i = 0; i < lost_chunk_indexes; i += 4) {
                    // Cast to uint32_t pointer before dereferencing to ensure correct size
                    uint32_t *packet = (uint32_t*)&send_buffer[sizeof(SwiftNetPacketInfo) + i];
                    printf("lost packet: %u\n", *packet);  // Use %u for unsigned integer
                }

                packet_info_new.packet_length = lost_chunk_indexes;
                packet_info_new.chunk_size = lost_chunk_indexes;

                memcpy(send_buffer, &packet_info_new, sizeof(SwiftNetPacketInfo));

                const uint32_t checksum = crc32(send_buffer, sizeof(SwiftNetPacketInfo) + lost_chunk_indexes);

                memcpy(&send_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));

                sendto(sockfd, send_buffer, sizeof(SwiftNetPacketInfo) + lost_chunk_indexes, 0x00, (const struct sockaddr *)&node->sender_address, node->sender_address_len);

                goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE:
            {
                volatile SwiftNetPacketSending* const target_packet_sending = get_packet_sending(packet_sending, packet_sending_size, packet_info.packet_id);

                printf("updated lost packets\n");

                if(unlikely(target_packet_sending == NULL)) {
                    goto next_packet;
                }

                if(unlikely(target_packet_sending->lost_chunks == NULL)) {
                    target_packet_sending->lost_chunks = malloc(maximum_transmission_unit);
                }

                memcpy((void*)target_packet_sending->lost_chunks, packet_data, packet_info.packet_length);

                target_packet_sending->lost_chunks_size = packet_info.packet_length / 4;
                target_packet_sending->updated_lost_chunks = true;

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

        const uint32_t mtu = MIN(packet_info.maximum_transmission_unit, maximum_transmission_unit);
        const uint32_t chunk_data_size = mtu - sizeof(SwiftNetPacketInfo);
        
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

                new_pending_message->chunks_received_number++;

                chunk_received(new_pending_message, packet_info.chunk_index);
                    
                memcpy(new_pending_message->packet_data_start, &packet_buffer[header_size], chunk_data_size);
            } else {
                current_read_pointer = packet_buffer + header_size;

                packet_completed(packet_info.packet_id, packet_info.packet_length, void_connection);

                (*packet_handler)(packet_buffer + header_size, packet_metadata);

                continue;
            }
        } else {
            const float percentage_completed = (float)(pending_message->chunks_received_number + 1) / packet_info.chunk_amount;

            const unsigned int bytes_to_copy = packet_info.chunk_amount == packet_info.chunk_index + 1 ? packet_info.packet_length % chunk_data_size : chunk_data_size;

            printf("completed: %f.2\n", percentage_completed);

            if(pending_message->chunks_received_number + 1 >= packet_info.chunk_amount) {
                    // Completed the packet
                memcpy(pending_message->packet_data_start + (chunk_data_size * packet_info.chunk_index), &packet_buffer[header_size], bytes_to_copy);

                current_read_pointer = pending_message->packet_data_start;

                packet_completed(packet_info.packet_id, packet_info.packet_length, void_connection);

                (*packet_handler)(pending_message->packet_data_start, packet_metadata);

                free(pending_message->packet_data_start);
                free(pending_message->chunks_received);

                memset(pending_message, 0x00, sizeof(SwiftNetPendingMessage));
            } else {
                memcpy(pending_message->packet_data_start + (chunk_data_size * packet_info.chunk_index), &packet_buffer[header_size], bytes_to_copy);

                chunk_received(pending_message, packet_info.chunk_index);

                pending_message->chunks_received_number++;
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
