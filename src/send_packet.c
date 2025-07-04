#include "swift_net.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include "internal/internal.h"

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
    const uint32_t maximum_transmission_unit,
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

    SwiftNetPacketInfo request_lost_packets_bit_array = {
        .packet_type = PACKET_TYPE_SEND_LOST_PACKETS_REQUEST,
        .packet_id = packet_sending->packet_id,
        .port_info = port_info,
        .packet_length = 0x00,
        .chunk_size = 0x00,
        .checksum = 0x00,
        .maximum_transmission_unit = maximum_transmission_unit
    };
 
    request_lost_packets_bit_array.checksum = crc32((uint8_t*)&request_lost_packets_bit_array, sizeof(request_lost_packets_bit_array));
 
    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;
    const uint32_t chunk_amount = (packet_length + (maximum_transmission_unit - PACKET_HEADER_SIZE) - 1) / (maximum_transmission_unit - PACKET_HEADER_SIZE);

    const SwiftNetPacketInfo resend_chunk_packet_info = {
        .packet_type = PACKET_TYPE_MESSAGE,
        .port_info = port_info,
        .packet_id = packet_sending->packet_id,
        .packet_length = packet_length,
        .chunk_amount = chunk_amount,
        .chunk_size = maximum_transmission_unit - PACKET_HEADER_SIZE,
        .checksum = 0x00,
        .maximum_transmission_unit = maximum_transmission_unit
    };

    uint8_t resend_chunk_buffer[maximum_transmission_unit - sizeof(struct ip)];

    memcpy(resend_chunk_buffer, &resend_chunk_packet_info, sizeof(SwiftNetPacketInfo));

    while(1) {
        const uint8_t request_lost_packets_bitarray_response = request_lost_packets_bitarray((uint8_t*)&request_lost_packets_bit_array, sizeof(SwiftNetPacketInfo), (const struct sockaddr*)destination_address, sockfd, packet_sending);

        switch (request_lost_packets_bitarray_response) {
            case REQUEST_LOST_PACKETS_RETURN_UPDATED_BIT_ARRAY:
                break;
            case REQUEST_LOST_PACKETS_RETURN_COMPLETED_PACKET:
                free((void*)packet_sending->lost_chunks);
                return;
        }
    
        for(uint32_t i = 0; i < packet_sending->lost_chunks_size; i++) {
            const uint32_t lost_chunk_index = packet_sending->lost_chunks[i];
    
            memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, chunk_index)], &lost_chunk_index, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_index));
    
            const uint32_t current_offset = lost_chunk_index * (maximum_transmission_unit - PACKET_HEADER_SIZE);

            if(current_offset + maximum_transmission_unit - PACKET_HEADER_SIZE > packet_length) {
                const uint32_t bytes_to_complete = packet_length - current_offset;

                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, packet_length)], &bytes_to_complete, sizeof(bytes_to_complete));

                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, chunk_size)], &bytes_to_complete, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_size));
                
                memcpy(&resend_chunk_buffer[sizeof(SwiftNetPacketInfo)], &packet->packet_data_start[current_offset], bytes_to_complete);
                
                memset(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], 0x00, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));

                const uint32_t checksum = crc32(resend_chunk_buffer, bytes_to_complete);

                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));
    
                sendto(sockfd, resend_chunk_buffer, bytes_to_complete, 0, (const struct sockaddr*)destination_address, *destination_address_len);
            } else {
                memcpy(&resend_chunk_buffer[sizeof(SwiftNetPacketInfo)], &packet->packet_data_start[current_offset], maximum_transmission_unit - PACKET_HEADER_SIZE);

                memset(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], 0x00, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));

                const uint32_t checksum = crc32(resend_chunk_buffer, maximum_transmission_unit - sizeof(struct ip));

                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));
    
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

    SwiftNetPacketInfo packet_info = {
        .packet_type = PACKET_TYPE_MESSAGE,
        .port_info = port_info,
        .packet_length = packet_length,
        .packet_id = packet_id,
        .chunk_size = mtu - PACKET_HEADER_SIZE,
        .checksum = 0x00,
        .maximum_transmission_unit = maximum_transmission_unit
    };

    SwiftNetDebug(
        if (check_debug_flag(DEBUG_PACKETS_SENDING)) {
            send_debug_message("Sending packet: {\"destination_ip_address\": \"%s\", \"destination_port\": %d, \"packet_length\": %d}\n", inet_ntoa(target_addr->sin_addr), port_info.destination_port, packet_length);
        }
    )

    memcpy(packet->packet_buffer_start, &packet_info, sizeof(SwiftNetPacketInfo));

    if(packet_length > mtu) {
        volatile SwiftNetPacketSending* const empty_packet_sending = get_empty_packet_sending((volatile SwiftNetPacketSending* const)packets_sending, MAX_PACKETS_SENDING);
        if(unlikely(empty_packet_sending == NULL)) {
            fprintf(stderr, "Failed to send a packet: exceeded maximum amount of sending packets at the same time\n");
            return;
        }

        const uint32_t chunk_amount = (packet_length + (mtu - PACKET_HEADER_SIZE) - 1) / (mtu - PACKET_HEADER_SIZE);
    
        empty_packet_sending->packet_id = packet_id;

        packet_info.chunk_amount = chunk_amount;

        uint8_t buffer[mtu - sizeof(struct ip)];

        memcpy(buffer, &packet_info, sizeof(SwiftNetPacketInfo));

        for(uint32_t i = 0; ; i++) {
            const uint32_t current_offset = i * (mtu - PACKET_HEADER_SIZE);

            SwiftNetDebug(
                if (check_debug_flag(DEBUG_PACKETS_SENDING)) {
                    send_debug_message("Sent chunk: {\"chunk_index\": %d}\n", i);
                }
            )

            memcpy(&buffer[offsetof(SwiftNetPacketInfo, chunk_index)], &i, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_index));
            
            memset(&buffer[offsetof(SwiftNetPacketInfo, checksum)], 0x00, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));
        
            if(current_offset + mtu > packet_info.packet_length) {
                // last chunk
                const uint32_t bytes_to_send = packet_length - current_offset;

                memcpy(&buffer[sizeof(SwiftNetPacketInfo)], packet->packet_data_start + current_offset, bytes_to_send);
                memcpy(&buffer[offsetof(SwiftNetPacketInfo, chunk_size)], &bytes_to_send, sizeof(bytes_to_send));

                const uint32_t checksum = crc32(buffer, bytes_to_send + sizeof(SwiftNetPacketInfo));
                memcpy(&buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));

                sendto(sockfd, buffer, bytes_to_send + sizeof(SwiftNetPacketInfo), 0x00, (const struct sockaddr *)target_addr, *target_addr_len);

                handle_lost_packets(packets_sending, mtu, packet, sockfd, target_addr, target_addr_len, port_info.source_port, port_info.destination_port);
                
                break;
            } else {
                memcpy(&buffer[sizeof(SwiftNetPacketInfo)], packet->packet_data_start + current_offset, mtu - PACKET_HEADER_SIZE);

                const uint32_t checksum = crc32(buffer, sizeof(buffer));

                memcpy(&buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));

                sendto(sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
            }
        }
    } else {
        const uint32_t chunk_amount = 1;
        const uint32_t chunk_index = 0;

        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, chunk_size), &packet_length, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_size));
        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, chunk_amount), &chunk_amount, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_amount));
        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, chunk_index), &chunk_index, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_index));

        const uint32_t checksum = crc32(packet->packet_buffer_start, packet_length + sizeof(SwiftNetPacketInfo));

        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, checksum), &checksum, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));

        sendto(sockfd, packet->packet_buffer_start, packet_length + sizeof(SwiftNetPacketInfo), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
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
