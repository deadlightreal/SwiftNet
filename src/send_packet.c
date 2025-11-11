#include "swift_net.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include "internal/internal.h"
#include <netinet/in.h>

static inline void lock_packet_sending(SwiftNetPacketSending* const packet_sending) {
    bool locked = false;
    while(!atomic_compare_exchange_strong_explicit(&packet_sending->locked, &locked, true, memory_order_acquire, memory_order_relaxed)) {
        locked = false;
    }
}

static inline void unlock_packet_sending(SwiftNetPacketSending* const packet_sending) {
    atomic_store_explicit(&packet_sending->locked, false, memory_order_release);
}

static inline uint8_t request_lost_packets_bitarray(const uint8_t* const raw_data, const uint32_t data_size, const struct sockaddr* const destination, const int sockfd, SwiftNetPacketSending* const packet_sending) {
    while(1) {
        if(check_debug_flag(DEBUG_LOST_PACKETS)) {
            send_debug_message("Requested list of lost packets: {\"packet_id\": %d}\n", packet_sending->packet_id);
        }

        sendto(sockfd, raw_data, data_size, 0, destination, sizeof(*destination));

        for(uint8_t times_checked = 0; times_checked < 0xFF; times_checked++) {
            const PacketSendingUpdated status = atomic_load_explicit(&packet_sending->updated, memory_order_acquire);

            switch (status) {
                case NO_UPDATE:
                    break;
                case UPDATED_LOST_CHUNKS:
                    atomic_store_explicit(&packet_sending->updated, NO_UPDATE, memory_order_release);
                    return REQUEST_LOST_PACKETS_RETURN_UPDATED_BIT_ARRAY;
                case SUCCESSFULLY_RECEIVED:
                    atomic_store_explicit(&packet_sending->updated, NO_UPDATE, memory_order_release);
                    return REQUEST_LOST_PACKETS_RETURN_COMPLETED_PACKET;
            }

            usleep(1000);
        }
    }
}

static inline void handle_lost_packets(
    SwiftNetPacketSending* const packet_sending,
    const uint32_t mtu,
    const SwiftNetPacketBuffer* const packet, 
    const int sockfd,
    const struct sockaddr_in* const destination_address,
    const socklen_t* destination_address_len,
    const uint16_t source_port,
    const uint16_t destination_port,
    SwiftNetMemoryAllocator* const packets_sending_memory_allocator,
    SwiftNetVector* const packets_sending
    #ifdef SWIFT_NET_REQUESTS
        , const bool response
        , const uint8_t packet_type
    #endif
) {
    const SwiftNetPortInfo port_info = {
        .source_port = source_port,
        .destination_port = destination_port
    };

    const struct ip request_lost_packets_ip_header = construct_ip_header(destination_address->sin_addr, PACKET_HEADER_SIZE, packet_sending->packet_id);

    SwiftNetPacketInfo request_lost_packets_bit_array = {
        .packet_type = PACKET_TYPE_SEND_LOST_PACKETS_REQUEST,
        .port_info = port_info,
        .packet_length = 0x00,
        .maximum_transmission_unit = mtu,
        .chunk_index = 0,
        .chunk_amount = 1
    };

    uint8_t request_lost_packets_buffer[PACKET_HEADER_SIZE];

    memcpy(request_lost_packets_buffer, &request_lost_packets_ip_header, sizeof(struct ip));
    memcpy(request_lost_packets_buffer + sizeof(struct ip), &request_lost_packets_bit_array, sizeof(SwiftNetPacketInfo));
 
    uint16_t checksum = crc16(request_lost_packets_buffer, sizeof(request_lost_packets_buffer));

    memcpy(request_lost_packets_buffer + offsetof(struct ip, ip_sum), &checksum, SIZEOF_FIELD(struct ip, ip_sum));
 
    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;
    const uint32_t chunk_amount = (packet_length + (mtu - PACKET_HEADER_SIZE) - 1) / (mtu - PACKET_HEADER_SIZE);

    const SwiftNetPacketInfo resend_chunk_packet_info = {
        #ifdef SWIFT_NET_REQUESTS
        .packet_type = packet_type,
        #else
        .packet_type = PACKET_TYPE_MESSAGE,
        #endif
        .port_info = port_info,
        .packet_length = packet_length,
        .chunk_amount = chunk_amount,
        .maximum_transmission_unit = maximum_transmission_unit
    };
 
    const struct ip resend_chunk_ip_header = construct_ip_header(destination_address->sin_addr, mtu, packet_sending->packet_id);

    uint8_t resend_chunk_buffer[mtu];

    memcpy(resend_chunk_buffer, &resend_chunk_ip_header, sizeof(struct ip));

    memcpy(resend_chunk_buffer + sizeof(struct ip), &resend_chunk_packet_info, sizeof(SwiftNetPacketInfo));

    while(1) {
        const uint8_t request_lost_packets_bitarray_response = request_lost_packets_bitarray(request_lost_packets_buffer, PACKET_HEADER_SIZE, (const struct sockaddr*)destination_address, sockfd, packet_sending);

        lock_packet_sending(packet_sending);

        switch (request_lost_packets_bitarray_response) {
            case REQUEST_LOST_PACKETS_RETURN_UPDATED_BIT_ARRAY:
                break;
            case REQUEST_LOST_PACKETS_RETURN_COMPLETED_PACKET:
                free((void*)packet_sending->lost_chunks);

                vector_lock(packets_sending);

                for (uint32_t i = 0; i < packets_sending->size; i++) {
                    if (((SwiftNetPacketSending*)vector_get(packets_sending, i))->packet_id == packet_sending->packet_id) {
                        vector_remove(packets_sending, i);

                        break;
                    }
                }

                vector_unlock(packets_sending);

                unlock_packet_sending(packet_sending);

                allocator_free(packets_sending_memory_allocator, packet_sending);

                return;
        }
    
        for(uint32_t i = 0; i < packet_sending->lost_chunks_size; i++) {
            const uint32_t lost_chunk_index = packet_sending->lost_chunks[i];

            if (check_debug_flag(DEBUG_LOST_PACKETS) == true) {
                send_debug_message("Packet lost: {\"packet_id\": %d, \"chunk index\": %d}\n", packet_sending->packet_id, lost_chunk_index);
            }
    
            memcpy(&resend_chunk_buffer[sizeof(struct ip) + offsetof(SwiftNetPacketInfo, chunk_index)], &lost_chunk_index, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_index));
    
            const uint32_t current_offset = lost_chunk_index * (mtu - PACKET_HEADER_SIZE);

            if(current_offset + mtu - PACKET_HEADER_SIZE >= packet_length) {
                const uint32_t bytes_to_complete = packet_length - current_offset;

                const uint16_t new_ip_len = bytes_to_complete + PACKET_HEADER_SIZE;
                memcpy(&resend_chunk_buffer[offsetof(struct ip, ip_len)], &new_ip_len, SIZEOF_FIELD(struct ip, ip_len));
                
                memcpy(&resend_chunk_buffer[PACKET_HEADER_SIZE], &packet->packet_data_start[current_offset], bytes_to_complete);
                
                memset(&resend_chunk_buffer[offsetof(struct ip, ip_sum)], 0x00, SIZEOF_FIELD(struct ip, ip_sum));

                const uint16_t checksum = crc16(resend_chunk_buffer, bytes_to_complete + PACKET_HEADER_SIZE);

                memcpy(&resend_chunk_buffer[offsetof(struct ip, ip_sum)], &checksum, SIZEOF_FIELD(struct ip, ip_sum));
    
                sendto(sockfd, resend_chunk_buffer, bytes_to_complete + PACKET_HEADER_SIZE, 0, (const struct sockaddr*)destination_address, *destination_address_len);
            } else {
                memcpy(&resend_chunk_buffer[PACKET_HEADER_SIZE], &packet->packet_data_start[current_offset], mtu - PACKET_HEADER_SIZE);

                memset(&resend_chunk_buffer[offsetof(struct ip, ip_sum)], 0x00, SIZEOF_FIELD(struct ip, ip_sum));

                const uint16_t checksum = crc16(resend_chunk_buffer, sizeof(resend_chunk_buffer));

                memcpy(&resend_chunk_buffer[offsetof(struct ip, ip_sum)], &checksum, SIZEOF_FIELD(struct ip, ip_sum));
    
                sendto(sockfd, resend_chunk_buffer, sizeof(resend_chunk_buffer), 0, (const struct sockaddr*)destination_address, *destination_address_len);
            }
        }

        unlock_packet_sending(packet_sending);
    }
}

inline void swiftnet_send_packet(
    const void* const connection,
    const uint32_t target_maximum_transmission_unit,
    const SwiftNetPortInfo port_info,
    const SwiftNetPacketBuffer* const packet,
    const uint32_t packet_length,
    const struct sockaddr_in* const target_addr,
    const socklen_t* const target_addr_len,
    SwiftNetVector* const packets_sending,
    SwiftNetMemoryAllocator* const packets_sending_memory_allocator,
    const int sockfd
    #ifdef SWIFT_NET_REQUESTS
        , RequestSent* const request_sent
        , const bool response
        , const uint16_t request_packet_id
    #endif
) {
    const uint32_t mtu = MIN(target_maximum_transmission_unit, maximum_transmission_unit);

    #ifdef SWIFT_NET_DEBUG
        if (check_debug_flag(DEBUG_PACKETS_SENDING)) {
            send_debug_message("Sending packet: {\"destination_ip_address\": \"%s\", \"destination_port\": %d, \"packet_length\": %d}\n", inet_ntoa(target_addr->sin_addr), port_info.destination_port, packet_length);
        }
    #endif

    #ifdef SWIFT_NET_REQUESTS
        uint16_t packet_id;
        if (response == true) {
            packet_id = request_packet_id;
        } else {
            packet_id = rand();
        }

        if (request_sent != NULL) {
            request_sent->packet_id = packet_id;

            vector_lock(&requests_sent);

            vector_push(&requests_sent, request_sent);

            vector_unlock(&requests_sent);
        }
    #else
        const uint16_t packet_id = rand();
    #endif

    #ifdef SWIFT_NET_REQUESTS
    const uint8_t packet_type = response ? PACKET_TYPE_RESPONSE : request_sent == NULL ? PACKET_TYPE_MESSAGE : PACKET_TYPE_REQUEST;
    #endif

    if(packet_length > mtu) {
        SwiftNetPacketInfo packet_info = {
            .port_info = port_info,
            .packet_length = packet_length,
            .maximum_transmission_unit = maximum_transmission_unit,
            .chunk_index = 0
            #ifdef SWIFT_NET_REQUESTS
                , .packet_type = packet_type
            #else
                , .packet_type = PACKET_TYPE_MESSAGE
            #endif
        };

        const struct ip ip_header = construct_ip_header(target_addr->sin_addr, mtu, packet_id);

        SwiftNetPacketSending* const new_packet_sending = allocator_allocate(packets_sending_memory_allocator);
        if(unlikely(new_packet_sending == NULL)) {
            fprintf(stderr, "Failed to send a packet: exceeded maximum amount of sending packets at the same time\n");
            return;
        }

        vector_lock(packets_sending);

        vector_push((SwiftNetVector*)packets_sending, (SwiftNetPacketSending*)new_packet_sending);

        vector_unlock(packets_sending);

        const uint32_t chunk_amount = (packet_length + (mtu - PACKET_HEADER_SIZE) - 1) / (mtu - PACKET_HEADER_SIZE);

        new_packet_sending->lost_chunks = NULL;
        new_packet_sending->locked = false;
        new_packet_sending->lost_chunks = NULL;
        new_packet_sending->lost_chunks_size = 0;
        new_packet_sending->packet_id = packet_id;

        packet_info.chunk_amount = chunk_amount;

        uint8_t buffer[mtu];

        memcpy(buffer, &ip_header, sizeof(ip_header));
        memcpy(buffer + sizeof(ip_header), &packet_info, sizeof(packet_info));

        for(uint32_t i = 0; ; i++) {
            const uint32_t current_offset = i * (mtu - PACKET_HEADER_SIZE);

            #ifdef SWIFT_NET_DEBUG
                if (check_debug_flag(DEBUG_PACKETS_SENDING)) {
                    send_debug_message("Sent chunk: {\"chunk_index\": %d}\n", i);
                }
            #endif

            memcpy(&buffer[sizeof(struct ip) + offsetof(SwiftNetPacketInfo, chunk_index)], &i, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_index));
            
            memset(&buffer[offsetof(struct ip, ip_sum)], 0x00, SIZEOF_FIELD(struct ip, ip_sum));
        
            if(current_offset + (mtu - PACKET_HEADER_SIZE) >= packet_info.packet_length) {
                // Last chunk
                const uint16_t bytes_to_send = (uint16_t)packet_length - current_offset + PACKET_HEADER_SIZE;

                memcpy(&buffer[PACKET_HEADER_SIZE], packet->packet_data_start + current_offset, bytes_to_send - PACKET_HEADER_SIZE);
                memcpy(&buffer[offsetof(struct ip, ip_len)], &bytes_to_send, SIZEOF_FIELD(struct ip, ip_len));

                const uint16_t checksum = crc16(buffer, bytes_to_send);

                memcpy(&buffer[offsetof(struct ip, ip_sum)], &checksum, SIZEOF_FIELD(struct ip, ip_sum));

                sendto(sockfd, buffer, bytes_to_send, 0x00, (const struct sockaddr *)target_addr, *target_addr_len);

                handle_lost_packets(new_packet_sending, mtu, packet, sockfd, target_addr, target_addr_len, port_info.source_port, port_info.destination_port, packets_sending_memory_allocator, packets_sending
                #ifdef SWIFT_NET_REQUESTS
                    , response
                    , packet_type
                #endif
                );
                
                break;
            } else {
                memcpy(buffer + PACKET_HEADER_SIZE, packet->packet_data_start + current_offset, mtu - PACKET_HEADER_SIZE);

                const uint16_t checksum = crc16(buffer, sizeof(buffer));

                memcpy(&buffer[offsetof(struct ip, ip_sum)], &checksum, SIZEOF_FIELD(struct ip, ip_sum));

                sendto(sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
            }
        }
    } else {
        const uint32_t final_packet_size = PACKET_HEADER_SIZE + packet_length;

        const SwiftNetPacketInfo packet_info = {
            .port_info = port_info,
            .packet_length = packet_length,
            .maximum_transmission_unit = maximum_transmission_unit,
            .chunk_amount = 1,
            #ifdef SWIFT_NET_REQUESTS
            .packet_type = packet_type
            #else
            .packet_type = PACKET_TYPE_MESSAGE
            #endif
        };

        const struct ip ip_header = construct_ip_header(target_addr->sin_addr, final_packet_size, packet_id);

        memcpy(packet->packet_buffer_start, &ip_header, sizeof(struct ip));
        memcpy(packet->packet_buffer_start + sizeof(struct ip), &packet_info, sizeof(SwiftNetPacketInfo));

        const uint16_t checksum = crc16(packet->packet_buffer_start, final_packet_size);

        memcpy(packet->packet_buffer_start + offsetof(struct ip, ip_sum), &checksum, SIZEOF_FIELD(struct ip, ip_sum));

        sendto(sockfd, packet->packet_buffer_start, final_packet_size, 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
    }
}

void swiftnet_client_send_packet(SwiftNetClientConnection* const client, SwiftNetPacketBuffer* const packet) {
    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;

    swiftnet_send_packet(client, client->maximum_transmission_unit, client->port_info, packet, packet_length, &client->server_addr, &client->server_addr_len, &client->packets_sending, &client->packets_sending_memory_allocator, client->sockfd
    #ifdef SWIFT_NET_REQUESTS
        , NULL, false, 0
    #endif
    );
}

void swiftnet_server_send_packet(SwiftNetServer* const server, SwiftNetPacketBuffer* const packet, const SwiftNetClientAddrData target) {
    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;

    const SwiftNetPortInfo port_info = {
        .destination_port = target.sender_address.sin_port,
        .source_port = server->server_port
    };

    swiftnet_send_packet(server, target.maximum_transmission_unit, port_info, packet, packet_length, &target.sender_address, &target.sender_address_length, &server->packets_sending, &server->packets_sending_memory_allocator, server->sockfd
    #ifdef SWIFT_NET_REQUESTS
        , NULL, false, 0
    #endif
    );
}
