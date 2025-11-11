#include "internal/internal.h"
#include "swift_net.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

static inline void lock_packet_sending(SwiftNetPacketSending* const packet_sending) {
    bool locked = false;
    while(!atomic_compare_exchange_strong_explicit(&packet_sending->locked, &locked, true, memory_order_acquire, memory_order_relaxed)) {
        locked = false;
    }
}

static inline void unlock_packet_sending(SwiftNetPacketSending* const packet_sending) {
    atomic_store_explicit(&packet_sending->locked, false, memory_order_release);
}

// Returns an array of 4 byte uint32_tegers, that contain indexes of lost chunks
static inline const uint32_t return_lost_chunk_indexes(const uint8_t* const chunks_received, const uint32_t chunk_amount, const uint32_t buffer_size, uint32_t* const buffer) {
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
            
            return offset;
        }

        byte++;
    }

    return offset;
}

static inline void packet_completed(const uint16_t packet_id, const uint32_t packet_length, SwiftNetVector* const packets_completed_history, SwiftNetMemoryAllocator* const packets_completed_history_memory_allocator) {
    SwiftNetPacketCompleted* const new_packet_completed = allocator_allocate(packets_completed_history_memory_allocator);
    new_packet_completed->packet_id = packet_id;
    new_packet_completed->packet_length = packet_length;

    vector_lock(packets_completed_history);

    vector_push(packets_completed_history, new_packet_completed);

    vector_unlock(packets_completed_history);

    return;
}

static inline bool check_packet_already_completed(const uint16_t packet_id, SwiftNetVector* const packets_completed_history) {
    vector_lock(packets_completed_history);

    for(uint32_t i = 0; i < packets_completed_history->size; i++) {
        if(((const SwiftNetPacketCompleted* restrict const)vector_get((SwiftNetVector*)packets_completed_history, i))->packet_id == packet_id) {
            vector_unlock(packets_completed_history);

            return true; 
        }
    }

    vector_unlock(packets_completed_history);

    return false;
}

static inline SwiftNetPendingMessage* const get_pending_message(const SwiftNetPacketInfo* const restrict packet_info, SwiftNetVector* const pending_messages_vector, const ConnectionType connection_type, const in_addr_t sender_address, const uint16_t packet_id) {
    vector_lock(pending_messages_vector);

    for(uint32_t i = 0; i < pending_messages_vector->size; i++) {
        SwiftNetPendingMessage* const current_pending_message = vector_get((SwiftNetVector*)pending_messages_vector, i);

        if((connection_type == CONNECTION_TYPE_CLIENT && current_pending_message->packet_id == packet_id) || (connection_type == CONNECTION_TYPE_SERVER && current_pending_message->sender_address == sender_address && current_pending_message->packet_id == packet_id)) {
            vector_unlock((SwiftNetVector*)pending_messages_vector);

            return current_pending_message;
        }
    }

    vector_unlock((SwiftNetVector*)pending_messages_vector);

    return NULL;
}

static inline void insert_callback_queue_node(PacketCallbackQueueNode* const new_node, PacketCallbackQueue* const packet_queue) {
    if(unlikely(new_node == NULL)) {
        return;
    }

    uint32_t owner_none = PACKET_CALLBACK_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong_explicit(&packet_queue->owner, &owner_none, PACKET_CALLBACK_QUEUE_OWNER_PROCESS_PACKETS, memory_order_acquire, memory_order_relaxed)) {
        owner_none = PACKET_CALLBACK_QUEUE_OWNER_NONE;
    }

    if(packet_queue->last_node == NULL) {
        packet_queue->last_node = new_node;
    } else {
        packet_queue->last_node->next = new_node;

        packet_queue->last_node = new_node;
    }

    if(packet_queue->first_node == NULL) {
        packet_queue->first_node = new_node;
    }

    atomic_store_explicit(&packet_queue->owner, PACKET_CALLBACK_QUEUE_OWNER_NONE, memory_order_release);

    return;
}

#ifdef SWIFT_NET_REQUESTS

static inline void handle_request_response(const uint16_t packet_id, const in_addr_t sender, SwiftNetPendingMessage* const pending_message, void* const packet_data, SwiftNetVector* const pending_messages, const ConnectionType connection_type) {
    bool is_valid_response = false;

    vector_lock(&requests_sent);

    for (uint32_t i = 0; i < requests_sent.size; i++) {
        RequestSent* const current_request_sent = vector_get(&requests_sent, i);

        if (current_request_sent == NULL) {
            continue;
        }

        if (current_request_sent->packet_id == packet_id) {
            const in_addr_t first_byte = (sender >> 0) & 0xFF;
            const in_addr_t second_byte = (sender >> 8) & 0xFF;

            if ((first_byte != 127) && !(first_byte == 192 && second_byte == 168)) {
                if (current_request_sent->address != sender) {
                    continue;
                }
            }

            atomic_store_explicit(&current_request_sent->packet_data, packet_data, memory_order_release);

            vector_remove(&requests_sent, i);

            is_valid_response = true;

            break;
        }
    }

    vector_unlock(&requests_sent);

    if (is_valid_response == true) {
        if (pending_message != NULL) {
            free(pending_message->chunks_received);
            
            allocator_free(&pending_message_memory_allocator, pending_message);

            vector_lock(pending_messages);

            for (uint32_t i = 0; i < pending_messages->size; i++) {
                const SwiftNetPendingMessage* const pending_message = vector_get(pending_messages, i);
                if ((connection_type == CONNECTION_TYPE_CLIENT && pending_message->packet_id == packet_id) || (connection_type == CONNECTION_TYPE_SERVER && pending_message->sender_address == sender && pending_message->packet_id == packet_id)) {
                    vector_remove(pending_messages, i);
                }
            }

            vector_unlock(pending_messages);
        }

        return;
    }
}

#endif

static inline void pass_callback_execution(void* const packet_data, PacketCallbackQueue* const queue, SwiftNetPendingMessage* const pending_message, const uint16_t packet_id
) {
    PacketCallbackQueueNode* const node = allocator_allocate(&packet_callback_queue_node_memory_allocator);
    node->packet_data = packet_data;
    node->next = NULL;
    node->pending_message = pending_message;
    node->packet_id = packet_id;

    atomic_thread_fence(memory_order_release);

    insert_callback_queue_node(node, queue);
}


static inline void chunk_received(uint8_t* const chunks_received, const uint32_t index) {
    const uint32_t byte = index / 8;
    const uint8_t bit = index % 8;

    chunks_received[byte] |= 1 << bit;
}

static inline SwiftNetPendingMessage* const create_new_pending_message(SwiftNetVector* const pending_messages, SwiftNetMemoryAllocator* const pending_messages_memory_allocator, const SwiftNetPacketInfo* const restrict packet_info, const ConnectionType connection_type, const in_addr_t sender_address, const uint16_t packet_id) {
    SwiftNetPendingMessage* const new_pending_message = allocator_allocate(pending_messages_memory_allocator);

    uint8_t* const allocated_memory = malloc(packet_info->packet_length);

    const uint32_t chunks_received_byte_size = (packet_info->chunk_amount + 7) / 8;

    new_pending_message->packet_info = *packet_info;

    new_pending_message->packet_data_start = allocated_memory;
    new_pending_message->chunks_received_number = 0x00;

    new_pending_message->chunks_received_length = chunks_received_byte_size;
    new_pending_message->chunks_received = calloc(chunks_received_byte_size, 1);

    new_pending_message->packet_id = packet_id;

    if(connection_type == CONNECTION_TYPE_SERVER) {
        new_pending_message->sender_address = sender_address;
    }

    vector_lock(pending_messages);

    vector_push((SwiftNetVector*)pending_messages, new_pending_message);

    vector_unlock(pending_messages);

    return new_pending_message;
}

static inline SwiftNetPacketSending* const get_packet_sending(SwiftNetVector* const packet_sending_array, const uint16_t target_id) {
    vector_lock(packet_sending_array);

    for(uint32_t i = 0; i < packet_sending_array->size; i++) {
        SwiftNetPacketSending* const current_packet_sending = vector_get((SwiftNetVector*)packet_sending_array, i);

        if(current_packet_sending->packet_id == target_id) {
            vector_unlock(packet_sending_array);

            return current_packet_sending;
        }
    }

    vector_unlock(packet_sending_array);

    return NULL;
}

PacketQueueNode* const wait_for_next_packet(PacketQueue* const packet_queue) {
    uint32_t owner_none = PACKET_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong_explicit(&packet_queue->owner, &owner_none, PACKET_QUEUE_OWNER_PROCESS_PACKETS, memory_order_acquire, memory_order_relaxed)) {
        owner_none = PACKET_QUEUE_OWNER_NONE;
    }

    if(packet_queue->first_node == NULL) {
        atomic_store(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE);
        return NULL;
    }

    PacketQueueNode* const node_to_process = packet_queue->first_node;

    if(node_to_process->next == NULL) {
        packet_queue->first_node = NULL;
        packet_queue->last_node = NULL;

        atomic_store(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE);

        return node_to_process;
    }

    packet_queue->first_node = node_to_process->next;

    atomic_store_explicit(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE, memory_order_release);

    return node_to_process;
}

static inline bool packet_corrupted(const uint16_t checksum, const uint32_t chunk_size, const uint8_t* const buffer) {
    return crc16(buffer, chunk_size) != checksum;
}

static inline void swiftnet_process_packets(
    void* _Atomic * packet_handler,
    const int sockfd,
    const uint16_t source_port,
    SwiftNetVector* const packets_sending,
    SwiftNetMemoryAllocator* const packets_sending_messages_memory_allocator,
    SwiftNetVector* const pending_messages,
    SwiftNetMemoryAllocator* const pending_messages_memory_allocator,
    SwiftNetVector* const packets_completed_history,
    SwiftNetMemoryAllocator* const packets_completed_history_memory_allocator,
    ConnectionType connection_type,
    PacketQueue* const packet_queue,
    PacketCallbackQueue* const packet_callback_queue,
    void* const connection,
    _Atomic bool* closing
) {
    while(1) {
        if (atomic_load(closing) == true) {
            break;
        }

        PacketQueueNode* const node = wait_for_next_packet(packet_queue);
        if(node == NULL) {
            continue;
        }

        atomic_thread_fence(memory_order_acquire);

        if(node->data_read == 0) {
            allocator_free(&packet_queue_node_memory_allocator, (void*)node);
            continue;
        }

        uint8_t* const packet_buffer = node->data;
        if(packet_buffer == NULL) {
            goto next_packet;
        }

        uint8_t* const packet_data = &packet_buffer[PACKET_HEADER_SIZE];

        // Check if user set a function that will execute with the packet data received as arg
        #ifdef SWIFT_NET_ERROR
            const void* const packet_handler_derenfernced = atomic_load(packet_handler);
            if(unlikely(packet_handler_derenfernced == NULL)) {
                allocator_free(&packet_queue_node_memory_allocator, (void*)node);
                allocator_free(&packet_buffer_memory_allocator, packet_buffer);
                fprintf(stderr, "Message Handler not set!!\n");
                continue;
            }
        #endif

        struct ip ip_header;
        memcpy(&ip_header, packet_buffer, sizeof(ip_header));

        SwiftNetPacketInfo packet_info;
        memcpy(&packet_info, &packet_buffer[sizeof(ip_header)], sizeof(packet_info));

        // Check if the packet is meant to be for this server
        if(packet_info.port_info.destination_port != source_port) {
            allocator_free(&packet_buffer_memory_allocator, packet_buffer);

            goto next_packet;
        }

        const uint16_t checksum_received = ip_header.ip_sum;

        memset(&packet_buffer[offsetof(struct ip, ip_sum)], 0x00, SIZEOF_FIELD(struct ip, ip_sum));

        memcpy(packet_buffer + offsetof(struct ip, ip_len), (void*)&node->data_read, SIZEOF_FIELD(struct ip, ip_len));

        if(memcmp(&ip_header.ip_src, &ip_header.ip_dst, sizeof(struct in_addr)) != 0) {
            if(ip_header.ip_sum != 0 && packet_corrupted(checksum_received, node->data_read, packet_buffer) == true) {
                #ifdef SWIFT_NET_DEBUG
                    if (check_debug_flag(DEBUG_PACKETS_RECEIVING)) {
                        send_debug_message("Received corrupted packet: {\"source_ip_address\": \"%s\", \"source_port\": %d, \"packet_id\": %d}\n", inet_ntoa(ip_header.ip_src), packet_info.port_info.source_port, ip_header.ip_id);
                    }
                #endif

                allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                goto next_packet;
            }
        }

        #ifdef SWIFT_NET_DEBUG
            if (check_debug_flag(DEBUG_PACKETS_RECEIVING)) {
                send_debug_message("Received packet: {\"source_ip_address\": \"%s\", \"source_port\": %d, \"packet_id\": %d, \"packet_type\": %d, \"packet_length\": %d, \"chunk_index\": %d}\n", inet_ntoa(ip_header.ip_src), packet_info.port_info.source_port, ip_header.ip_id, packet_info.packet_type, packet_info.packet_length, packet_info.chunk_index);
            }
        #endif

        switch(packet_info.packet_type) {
            case PACKET_TYPE_REQUEST_INFORMATION:
            {
                    const struct ip send_server_info_ip_header = construct_ip_header(node->sender_address.sin_addr, PACKET_HEADER_SIZE, rand());

                    const SwiftNetPacketInfo packet_info_new = {
                        .port_info = (SwiftNetPortInfo){
                            .source_port = source_port,
                            .destination_port = packet_info.port_info.source_port
                        },
                        .packet_type = PACKET_TYPE_REQUEST_INFORMATION,
                        .packet_length = sizeof(SwiftNetServerInformation),
                        .chunk_amount = 1,
                        .maximum_transmission_unit = maximum_transmission_unit
                    };

                    uint8_t send_buffer[PACKET_HEADER_SIZE];
        
                    memcpy(send_buffer, &send_server_info_ip_header, sizeof(send_server_info_ip_header));
                    memcpy(send_buffer + sizeof(struct ip), &packet_info_new, sizeof(packet_info_new));

                    const uint16_t checksum = crc16(send_buffer, sizeof(send_buffer));

                    memcpy(&send_buffer[offsetof(struct ip, ip_sum)], &checksum, SIZEOF_FIELD(struct ip, ip_sum));

                    sendto(sockfd, send_buffer, sizeof(send_buffer), 0, (struct sockaddr *)&node->sender_address, node->server_address_length);

                    allocator_free(&packet_buffer_memory_allocator, packet_buffer);
        
                    goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_REQUEST:
            {
                const uint32_t mtu = MIN(packet_info.maximum_transmission_unit, maximum_transmission_unit);

                SwiftNetPendingMessage* const pending_message = get_pending_message(&packet_info, pending_messages, connection_type, ip_header.ip_src.s_addr, ip_header.ip_id);
                if(pending_message == NULL) {
                    const bool packet_already_completed = check_packet_already_completed(ip_header.ip_id, packets_completed_history);
                    if(likely(packet_already_completed == true)) {
                        const struct ip send_packet_ip_header = construct_ip_header(node->sender_address.sin_addr, PACKET_HEADER_SIZE, ip_header.ip_id);

                        SwiftNetPacketInfo send_packet_info = {
                            .packet_length = 0x00,
                            .chunk_amount = 1,
                            .packet_type = PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET,
                            .port_info = (SwiftNetPortInfo){
                                .destination_port = packet_info.port_info.source_port,
                                .source_port = packet_info.port_info.destination_port
                            },
                            .maximum_transmission_unit = maximum_transmission_unit,
                            .chunk_index = 0
                        };

                        uint8_t send_buffer[PACKET_HEADER_SIZE];

                        memcpy(send_buffer, &send_packet_ip_header, sizeof(send_packet_ip_header));
                        memcpy(send_buffer + sizeof(send_packet_ip_header), &send_packet_info, sizeof(send_packet_info));

                        const uint16_t checksum = crc16(send_buffer, sizeof(send_buffer));

                        memcpy(send_buffer + offsetof(struct ip, ip_sum), &checksum, SIZEOF_FIELD(struct ip, ip_sum));

                        sendto(sockfd, &send_buffer, sizeof(send_buffer), 0x00, (const struct sockaddr *)&node->sender_address, node->server_address_length);

                        allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                        goto next_packet;
                    }

                    allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                    goto next_packet;
                }

                struct ip send_lost_packets_ip_header = construct_ip_header(node->sender_address.sin_addr, 0, ip_header.ip_id);

                SwiftNetPacketInfo packet_info_new = {
                    .port_info = (SwiftNetPortInfo){
                        .destination_port = packet_info.port_info.source_port,
                        .source_port = packet_info.port_info.destination_port
                    },
                    .packet_type = PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE,
                    .chunk_amount = 1,
                    .chunk_index = 0,
                    .maximum_transmission_unit = maximum_transmission_unit
                };

                uint8_t send_buffer[mtu];
                memset(send_buffer, 0, mtu);
                const uint32_t lost_chunk_indexes = return_lost_chunk_indexes(pending_message->chunks_received, pending_message->packet_info.chunk_amount, mtu - PACKET_HEADER_SIZE, (uint32_t*)(send_buffer + PACKET_HEADER_SIZE));

                packet_info_new.packet_length = lost_chunk_indexes * sizeof(uint32_t);

                send_lost_packets_ip_header.ip_len = lost_chunk_indexes + PACKET_HEADER_SIZE;

                const uint32_t packet_length = PACKET_HEADER_SIZE + (lost_chunk_indexes * sizeof(uint32_t));

                memcpy(send_buffer, &send_lost_packets_ip_header, sizeof(send_lost_packets_ip_header));
                memcpy(send_buffer + sizeof(struct ip), &packet_info_new, sizeof(packet_info_new));

                memcpy(send_buffer + offsetof(struct ip, ip_len), &packet_length, SIZEOF_FIELD(struct ip, ip_len));

                const uint16_t checksum = crc16(send_buffer, packet_length);

                memcpy(&send_buffer[offsetof(struct ip, ip_sum)], &checksum, SIZEOF_FIELD(struct ip, ip_sum));

                sendto(sockfd, send_buffer, packet_length, 0x00, (const struct sockaddr *)&node->sender_address, node->server_address_length);

                allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                goto next_packet;
            }
            case PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE:
            {
                SwiftNetPacketSending* const target_packet_sending = get_packet_sending(packets_sending, ip_header.ip_id);

                if(unlikely(target_packet_sending == NULL)) {
                    allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                    goto next_packet;
                }

                lock_packet_sending(target_packet_sending);

                if(target_packet_sending->lost_chunks == NULL) {
                    target_packet_sending->lost_chunks = malloc(maximum_transmission_unit - PACKET_HEADER_SIZE);
                }

                const uint32_t packets_lost = (packet_info.packet_length) / sizeof(uint32_t);

                memcpy((void*)target_packet_sending->lost_chunks, packet_data, packet_info.packet_length);

                target_packet_sending->lost_chunks_size = packet_info.packet_length / 4;

                atomic_store_explicit(&target_packet_sending->updated, UPDATED_LOST_CHUNKS, memory_order_release);

                allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                unlock_packet_sending(target_packet_sending);

                goto next_packet;
            }
            case PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET:
            {
                SwiftNetPacketSending* const target_packet_sending = get_packet_sending(packets_sending, ip_header.ip_id);

                if(unlikely(target_packet_sending == NULL)) {
                    allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                    goto next_packet;
                }

                atomic_store_explicit(&target_packet_sending->updated, SUCCESSFULLY_RECEIVED, memory_order_release);

                allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                goto next_packet;
            }
            default:
                break;
        }

        node->sender_address.sin_port = packet_info.port_info.source_port;

        const SwiftNetClientAddrData sender = {
            .sender_address = node->sender_address,
            .sender_address_length = node->server_address_length,
            .maximum_transmission_unit = packet_info.maximum_transmission_unit
        };

        const uint32_t mtu = MIN(packet_info.maximum_transmission_unit, maximum_transmission_unit);
        const uint32_t chunk_data_size = mtu - PACKET_HEADER_SIZE;

        SwiftNetPendingMessage* const pending_message = get_pending_message(&packet_info, pending_messages, connection_type, node->sender_address.sin_addr.s_addr, ip_header.ip_id);

        if(pending_message == NULL) {
            if(packet_info.packet_length + PACKET_HEADER_SIZE > mtu) {
                // Split packet into chunks
                SwiftNetPendingMessage* const new_pending_message = create_new_pending_message(pending_messages, pending_messages_memory_allocator, &packet_info, connection_type, node->sender_address.sin_addr.s_addr, ip_header.ip_id);

                new_pending_message->chunks_received_number++;

                chunk_received(new_pending_message->chunks_received, packet_info.chunk_index);
                    
                memcpy(new_pending_message->packet_data_start, &packet_buffer[PACKET_HEADER_SIZE], chunk_data_size);

                allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                goto next_packet;
            } else {
                packet_completed(ip_header.ip_id, packet_info.packet_length, packets_completed_history, packets_completed_history_memory_allocator);

                if(connection_type == CONNECTION_TYPE_SERVER) {
                    uint8_t* const ptr = packet_buffer + PACKET_HEADER_SIZE;

                    SwiftNetServerPacketData* const packet_data = allocator_allocate(&server_packet_data_memory_allocator) ;
                    packet_data->data = ptr;
                    packet_data->current_pointer = ptr;
                    packet_data->metadata = (SwiftNetPacketServerMetadata){
                        .port_info = packet_info.port_info,
                        .sender = sender,
                        .data_length = packet_info.packet_length,
                        .packet_id = ip_header.ip_id
                        #ifdef SWIFT_NET_REQUESTS
                            , .expecting_response = packet_info.packet_type == PACKET_TYPE_REQUEST
                        #endif
                    };

                    #ifdef SWIFT_NET_REQUESTS
                    if (packet_info.packet_type == PACKET_TYPE_RESPONSE) {
                        handle_request_response(ip_header.ip_id, sender.sender_address.sin_addr.s_addr, NULL, packet_data, pending_messages, connection_type);
                    } else {
                        pass_callback_execution(packet_data, packet_callback_queue, NULL, ip_header.ip_id);
                    }
                    #else
                        pass_callback_execution(packet_data, packet_callback_queue, NULL, ip_header.ip_id);
                    #endif
                } else {
                    uint8_t* const ptr = packet_buffer + PACKET_HEADER_SIZE;

                    SwiftNetClientPacketData* const packet_data = allocator_allocate(&client_packet_data_memory_allocator) ;
                    packet_data->data = ptr;
                    packet_data->current_pointer = ptr;
                    packet_data->metadata = (SwiftNetPacketClientMetadata){
                        .port_info = packet_info.port_info,
                        .data_length = packet_info.packet_length,
                        .packet_id = ip_header.ip_id
                        #ifdef SWIFT_NET_REQUESTS
                            , .expecting_response = packet_info.packet_type == PACKET_TYPE_REQUEST
                        #endif
                    };

                    #ifdef SWIFT_NET_REQUESTS
                    if (packet_info.packet_type == PACKET_TYPE_RESPONSE) {
                        handle_request_response(ip_header.ip_id, ((SwiftNetClientConnection*)connection)->server_addr.sin_addr.s_addr, NULL, packet_data, pending_messages, connection_type);
                    } else {
                        pass_callback_execution(packet_data, packet_callback_queue, NULL, ip_header.ip_id);
                    }
                    #else
                        pass_callback_execution(packet_data, packet_callback_queue, NULL, ip_header.ip_id);
                    #endif
                }

                goto next_packet;
            }
        } else {
            const uint32_t bytes_to_write = (packet_info.chunk_index + 1) >= packet_info.chunk_amount ? packet_info.packet_length % chunk_data_size : chunk_data_size;

            if(pending_message->chunks_received_number + 1 >= packet_info.chunk_amount) {
                // Completed the packet
                memcpy(pending_message->packet_data_start + (chunk_data_size * packet_info.chunk_index), &packet_buffer[PACKET_HEADER_SIZE], bytes_to_write);

                chunk_received(pending_message->chunks_received, packet_info.chunk_index);

                #ifdef SWIFT_NET_DEBUG
                    uint32_t lost_chunks_buffer[mtu - PACKET_HEADER_SIZE];

                    const uint32_t lost_chunks_num = return_lost_chunk_indexes(pending_message->chunks_received, packet_info.chunk_amount, mtu - PACKET_HEADER_SIZE, (uint32_t*)lost_chunks_buffer);

                    if (lost_chunks_num != 0) {
                        fprintf(stderr, "Packet marked as completed, but %d chunks are missing\n", lost_chunks_num);

                        for (uint32_t i = 0; i < lost_chunks_num; i++) {
                            printf("chunk index missing: %d\n", *(lost_chunks_buffer + i));  
                        }
                    }
                #endif

                packet_completed(ip_header.ip_id, packet_info.packet_length, packets_completed_history, packets_completed_history_memory_allocator);

                if(connection_type == CONNECTION_TYPE_SERVER) {
                    uint8_t* const ptr = pending_message->packet_data_start;

                    SwiftNetServerPacketData* const packet_data = allocator_allocate(&server_packet_data_memory_allocator);
                    packet_data->data = ptr;
                    packet_data->current_pointer = ptr;
                    packet_data->metadata = (SwiftNetPacketServerMetadata){
                        .port_info = packet_info.port_info,
                        .sender = sender,
                        .data_length = packet_info.packet_length,
                        .packet_id = ip_header.ip_id
                        #ifdef SWIFT_NET_REQUESTS
                            , .expecting_response = packet_info.packet_type == PACKET_TYPE_REQUEST
                        #endif
                    };

                    #ifdef SWIFT_NET_REQUESTS
                    if (packet_info.packet_type == PACKET_TYPE_RESPONSE) {
                        handle_request_response(ip_header.ip_id, sender.sender_address.sin_addr.s_addr, pending_message, packet_data, pending_messages, connection_type);
                    } else {
                        pass_callback_execution(packet_data, packet_callback_queue, pending_message, ip_header.ip_id);
                    }
                    #else
                        pass_callback_execution(packet_data, packet_callback_queue, pending_message, ip_header.ip_id);
                    #endif
                } else {
                    uint8_t* const ptr = pending_message->packet_data_start;

                    SwiftNetClientPacketData* const packet_data = allocator_allocate(&client_packet_data_memory_allocator) ;
                    packet_data->data = ptr;
                    packet_data->current_pointer = ptr;
                    packet_data->metadata = (SwiftNetPacketClientMetadata){
                        .port_info = packet_info.port_info,
                        .data_length = packet_info.packet_length,
                        .packet_id = ip_header.ip_id
                        #ifdef SWIFT_NET_REQUESTS
                            , .expecting_response = packet_info.packet_type == PACKET_TYPE_REQUEST
                        #endif
                    };

                    #ifdef SWIFT_NET_REQUESTS
                    if (packet_info.packet_type == PACKET_TYPE_RESPONSE) {
                        handle_request_response(ip_header.ip_id, ((SwiftNetClientConnection*)connection)->server_addr.sin_addr.s_addr, pending_message, packet_data, pending_messages, connection_type);
                    } else {
                        pass_callback_execution(packet_data, packet_callback_queue, pending_message, ip_header.ip_id);
                    }
                    #else
                        pass_callback_execution(packet_data, packet_callback_queue, pending_message, ip_header.ip_id);
                    #endif
                }

                allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                goto next_packet;
            } else {
                memcpy(pending_message->packet_data_start + (chunk_data_size * packet_info.chunk_index), &packet_buffer[PACKET_HEADER_SIZE], bytes_to_write);

                chunk_received(pending_message->chunks_received, packet_info.chunk_index);

                pending_message->chunks_received_number++;

                atomic_thread_fence(memory_order_release);

                allocator_free(&packet_buffer_memory_allocator, packet_buffer);

                goto next_packet;
            }
        }

        goto next_packet;

    next_packet:
        allocator_free(&packet_queue_node_memory_allocator, (void*)node);

        continue;
    }
}

void* swiftnet_server_process_packets(void* const void_server) {
    SwiftNetServer* const server = (SwiftNetServer*)void_server;

    swiftnet_process_packets((void*)&server->packet_handler, server->sockfd, server->server_port, &server->packets_sending, &server->packets_sending_memory_allocator, &server->pending_messages, &server->pending_messages_memory_allocator, &server->packets_completed, &server->packets_completed_memory_allocator, CONNECTION_TYPE_SERVER, &server->packet_queue, &server->packet_callback_queue, server, &server->closing);

    return NULL;
}

void* swiftnet_client_process_packets(void* const void_client) {
    SwiftNetClientConnection* const client = (SwiftNetClientConnection*)void_client;

    swiftnet_process_packets((void*)&client->packet_handler, client->sockfd, client->port_info.source_port, &client->packets_sending, &client->packets_sending_memory_allocator, &client->pending_messages, &client->pending_messages_memory_allocator, &client->packets_completed, &client->packets_completed_memory_allocator, CONNECTION_TYPE_CLIENT, &client->packet_queue, &client->packet_callback_queue, client, &client->closing);

    return NULL;
}
