#include "swift_net.h"
#include <_printf.h>
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

// These functions send the data from the packet buffer to the designated client or server.
static inline volatile SwiftNetPacketSending* get_empty_packet_sending(volatile SwiftNetPacketSending* const packet_sending_array, const uint16_t size) {
    for(uint16_t i = 0; i < size; i++) {
        volatile SwiftNetPacketSending* const current_packet_sending = &packet_sending_array[i];

        if(current_packet_sending->packet_id == 0x00) {
            return current_packet_sending;
        }
    }

    return NULL;
}

static inline uint8_t request_lost_packets_bitarray(const uint8_t* restrict const raw_data, const uint32_t data_size, const struct sockaddr* restrict const destination, const int sockfd, volatile SwiftNetPacketSending* const packet_sending) {
    while(1) {
        if(check_debug_flag(DEBUG_LOST_PACKETS)) {
            send_debug_message("Requested list of lost packets: {\"packet_id\": %d}\n", packet_sending->packet_id);
        }

        sendto(sockfd, raw_data, data_size, 0, destination, sizeof(*destination));

        for(uint8_t times_checked = 0; times_checked < 0xFF; times_checked++) {
            if(packet_sending->updated_lost_chunks == true) {
                packet_sending->updated_lost_chunks = false;
                return REQUEST_LOST_PACKETS_RETURN_UPDATED_BIT_ARRAY;
            }

            if(packet_sending->successfully_received == true) {
                return REQUEST_LOST_PACKETS_RETURN_COMPLETED_PACKET;
            }

            usleep(10000);
        }
    }
}

static inline void handle_lost_packets(
    volatile SwiftNetPacketSending* const packet_sending,
    const uint32_t mtu,
    const volatile SwiftNetPacketBuffer* const packet, 
    const int sockfd,
    const struct sockaddr_in* restrict const destination_address,
    const socklen_t* destination_address_len,
    const uint16_t source_port,
    const uint16_t destination_port
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
        .chunk_index = 0
    };

    uint8_t request_lost_packets_buffer[PACKET_HEADER_SIZE];

    memcpy(request_lost_packets_buffer, &request_lost_packets_ip_header, sizeof(struct ip));
    memcpy(request_lost_packets_buffer + sizeof(struct ip), &request_lost_packets_bit_array, sizeof(SwiftNetPacketInfo));
 
    uint16_t checksum = crc16(request_lost_packets_buffer, sizeof(request_lost_packets_buffer));

    memcpy(request_lost_packets_buffer + offsetof(struct ip, ip_sum), &checksum, SIZEOF_FIELD(struct ip, ip_sum));
 
    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;
    const uint32_t chunk_amount = (packet_length + (mtu - PACKET_HEADER_SIZE) - 1) / (mtu - PACKET_HEADER_SIZE);

    const SwiftNetPacketInfo resend_chunk_packet_info = {
        .packet_type = PACKET_TYPE_MESSAGE,
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
        const uint8_t request_lost_packets_bitarray_response = request_lost_packets_bitarray((uint8_t*)&request_lost_packets_buffer, PACKET_HEADER_SIZE, (const struct sockaddr*)destination_address, sockfd, packet_sending);

        switch (request_lost_packets_bitarray_response) {
            case REQUEST_LOST_PACKETS_RETURN_UPDATED_BIT_ARRAY:
                break;
            case REQUEST_LOST_PACKETS_RETURN_COMPLETED_PACKET:
                free((void*)packet_sending->lost_chunks);
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
                
                memset(&resend_chunk_buffer[offsetof(struct ip, ip_sum)], 0x00, SIZEOF_FIELD(struct ip, ip_len));

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
    }
}

static inline void swiftnet_send_packet(
    const void* restrict const connection,
    const uint32_t target_maximum_transmission_unit,
    const SwiftNetPortInfo port_info,
    const volatile SwiftNetPacketBuffer* const packet,
    const uint32_t packet_length,
    const struct sockaddr_in* restrict const target_addr,
    const socklen_t* restrict const target_addr_len,
    volatile SwiftNetPacketSending* const packets_sending,
    const int sockfd
) {
    const uint16_t packet_id = rand();

    const uint32_t mtu = MIN(target_maximum_transmission_unit, maximum_transmission_unit);

    SwiftNetDebug(
        if (check_debug_flag(DEBUG_PACKETS_SENDING)) {
            send_debug_message("Sending packet: {\"destination_ip_address\": \"%s\", \"destination_port\": %d, \"packet_length\": %d}\n", inet_ntoa(target_addr->sin_addr), port_info.destination_port, packet_length);
        }
    )

    if(packet_length > mtu) {
        SwiftNetPacketInfo packet_info = {
            .packet_type = PACKET_TYPE_MESSAGE,
            .port_info = port_info,
            .packet_length = packet_length,
            .maximum_transmission_unit = maximum_transmission_unit,
            .chunk_index = 0
        };

        const struct ip ip_header = construct_ip_header(target_addr->sin_addr, mtu, packet_id);

        volatile SwiftNetPacketSending* const empty_packet_sending = get_empty_packet_sending((volatile SwiftNetPacketSending* const)packets_sending, MAX_PACKETS_SENDING);
        if(unlikely(empty_packet_sending == NULL)) {
            fprintf(stderr, "Failed to send a packet: exceeded maximum amount of sending packets at the same time\n");
            return;
        }

        const uint32_t chunk_amount = (packet_length + (mtu - PACKET_HEADER_SIZE) - 1) / (mtu - PACKET_HEADER_SIZE);
    
        empty_packet_sending->packet_id = packet_id;

        packet_info.chunk_amount = chunk_amount;

        uint8_t buffer[mtu];

        memcpy(buffer, &ip_header, sizeof(ip_header));
        memcpy(buffer + sizeof(ip_header), &packet_info, sizeof(packet_info));

        for(uint32_t i = 0; ; i++) {
            const uint32_t current_offset = i * (mtu - PACKET_HEADER_SIZE);

            SwiftNetDebug(
                if (check_debug_flag(DEBUG_PACKETS_SENDING)) {
                    send_debug_message("Sent chunk: {\"chunk_index\": %d}\n", i);
                }
            )

            memcpy(&buffer[sizeof(struct ip) + offsetof(SwiftNetPacketInfo, chunk_index)], &i, SIZEOF_FIELD(struct ip, ip_off));
            
            memset(&buffer[offsetof(struct ip, ip_sum)], 0x00, SIZEOF_FIELD(struct ip, ip_sum));
        
            if(current_offset + (mtu - PACKET_HEADER_SIZE) >= packet_info.packet_length) {
                // Last chunk
                const uint16_t bytes_to_send = (uint16_t)packet_length - current_offset + PACKET_HEADER_SIZE;

                memcpy(&buffer[PACKET_HEADER_SIZE], packet->packet_data_start + current_offset, bytes_to_send - PACKET_HEADER_SIZE);
                memcpy(&buffer[offsetof(struct ip, ip_len)], &bytes_to_send, sizeof(bytes_to_send));

                const uint16_t checksum = crc16(buffer, bytes_to_send);

                memcpy(&buffer[offsetof(struct ip, ip_sum)], &checksum, sizeof(checksum));

                sendto(sockfd, buffer, bytes_to_send, 0x00, (const struct sockaddr *)target_addr, *target_addr_len);

                handle_lost_packets(packets_sending, mtu, packet, sockfd, target_addr, target_addr_len, port_info.source_port, port_info.destination_port);
                
                break;
            } else {
                memcpy(buffer + PACKET_HEADER_SIZE, packet->packet_data_start + current_offset, mtu - PACKET_HEADER_SIZE);

                const uint16_t checksum = crc16(buffer, sizeof(buffer));

                memcpy(&buffer[offsetof(struct ip, ip_sum)], &checksum, sizeof(checksum));

                sendto(sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
            }
        }
    } else {
        const uint32_t final_packet_size = PACKET_HEADER_SIZE + packet_length;

        const SwiftNetPacketInfo packet_info = {
            .packet_type = PACKET_TYPE_MESSAGE,
            .port_info = port_info,
            .packet_length = packet_length,
            .maximum_transmission_unit = maximum_transmission_unit,
            .chunk_amount = 1,
        };

        const struct ip ip_header = construct_ip_header(target_addr->sin_addr, final_packet_size, packet_id);

        memcpy(packet->packet_buffer_start, &ip_header, sizeof(struct ip));
        memcpy(packet->packet_buffer_start + sizeof(struct ip), &packet_info, sizeof(SwiftNetPacketInfo));

        const uint16_t checksum = crc16(packet->packet_buffer_start, final_packet_size);

        memcpy(packet->packet_buffer_start + offsetof(struct ip, ip_sum), &checksum, SIZEOF_FIELD(struct ip, ip_sum));

        sendto(sockfd, packet->packet_buffer_start, final_packet_size, 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
    }
}

void swiftnet_client_send_packet(SwiftNetClientConnection* restrict const client, SwiftNetPacketBuffer* restrict const packet) {
    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;

    swiftnet_send_packet(client, client->maximum_transmission_unit, client->port_info, packet, packet_length, &client->server_addr, &client->server_addr_len, client->packets_sending, client->sockfd);
}

void swiftnet_server_send_packet(SwiftNetServer* restrict const server, SwiftNetPacketBuffer* restrict const packet, const SwiftNetClientAddrData target) {
    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;

    const SwiftNetPortInfo port_info = {
        .destination_port = target.sender_address.sin_port,
        .source_port = server->server_port
    };

    swiftnet_send_packet(server, target.maximum_transmission_unit, port_info, packet, packet_length, &target.sender_address, &target.sender_address_length, server->packets_sending, server->sockfd);
}
