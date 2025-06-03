#include "swift_net.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include "internal/internal.h"

// These functions send the data from the packet buffer to the designated client or server.
static inline void null_check_connection(const void* restrict const ptr) { if(unlikely(ptr == NULL)) { fprintf(stderr, "Error: First argument ( Client | Server ) in send packet function is NULL!!\n"); exit(EXIT_FAILURE); } } static inline volatile SwiftNetPacketSending* get_empty_packet_sending(volatile SwiftNetPacketSending* const packet_sending_array, const uint16_t size) { for(uint16_t i = 0; i < size; i++) {
        volatile SwiftNetPacketSending* const current_packet_sending = &packet_sending_array[i];

        if(current_packet_sending->packet_id == 0x00) {
            return current_packet_sending;
        }
    }

    return NULL;
}

static inline uint8_t request_lost_packets_bitarray(const uint8_t* restrict const raw_data, const unsigned int data_size, const struct sockaddr* restrict const destination, const socklen_t socklen, const int sockfd, volatile SwiftNetPacketSending* const packet_sending) {
    while(1) {
        printf("requesting\n");
        sendto(sockfd, raw_data, data_size, 0, destination, socklen);

        for(uint8_t times_checked = 0; times_checked < 0xFF; times_checked++) {
            if(packet_sending->updated_lost_chunks == true) {
                packet_sending->updated_lost_chunks = false;
                return REQUEST_LOST_PACKETS_RETURN_UPDATED_BIT_ARRAY;
            }

            if(packet_sending->successfully_received == true) {
                printf("successfully_received\n");
                return REQUEST_LOST_PACKETS_RETURN_COMPLETED_PACKET;
            }

            usleep(10000);
        }
    }
}

static inline void handle_lost_packets(volatile SwiftNetPacketSending* const packet_sending, const unsigned int maximum_transmission_unit, const volatile CONNECTION_TYPE* const connection EXTRA_SERVER_ARG(const SwiftNetClientAddrData* restrict const destination)) {
    SwiftNetServerCode(
        const uint16_t source_port = connection->server_port;
        const int sockfd = connection->sockfd;
        const uint16_t destination_port = destination->client_address.sin_port;
        const volatile SwiftNetPacket* const packet = &connection->packet;

        const struct sockaddr* destination_address = (struct sockaddr * restrict)&destination->client_address;
        const socklen_t socklen = destination->client_address_length;
        const uint16_t mtu = MIN(destination->maximum_transmission_unit, maximum_transmission_unit);
    )

    SwiftNetClientCode(
        const uint16_t source_port = connection->port_info.source_port;
        const int sockfd = connection->sockfd;
        const uint16_t destination_port = connection->port_info.destination_port;
        const volatile SwiftNetPacket* const packet = &connection->packet;

        const struct sockaddr* restrict destination_address = (const struct sockaddr* restrict)&connection->server_addr;
        const socklen_t socklen = sizeof(connection->server_addr);
        const uint16_t mtu = MIN(connection->maximum_transmission_unit, maximum_transmission_unit);
    )

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
 
    request_lost_packets_bit_array.checksum =  crc32((uint8_t*)&request_lost_packets_bit_array, sizeof(request_lost_packets_bit_array));
 
    const unsigned int packet_length = packet->packet_append_pointer - packet->packet_data_start;
    const unsigned int chunk_amount = packet_length / (maximum_transmission_unit - sizeof(SwiftNetPacketInfo));

    const SwiftNetPacketInfo resend_chunk_packet_info = {
        .packet_type = PACKET_TYPE_MESSAGE,
        .port_info = port_info,
        .packet_id = packet_sending->packet_id,
        .packet_length = packet_length,
        .chunk_amount = chunk_amount,
        .chunk_size = mtu - sizeof(SwiftNetPacketInfo),
        .checksum = 0x00,
        .maximum_transmission_unit = maximum_transmission_unit
    };

    uint8_t resend_chunk_buffer[maximum_transmission_unit];

    memcpy(resend_chunk_buffer, &resend_chunk_packet_info, sizeof(SwiftNetPacketInfo));

    while(1) {
        uint8_t request_lost_packets_bitarray_response = request_lost_packets_bitarray((uint8_t*)&request_lost_packets_bit_array, sizeof(SwiftNetPacketInfo), destination_address, socklen, sockfd, packet_sending);

        switch (request_lost_packets_bitarray_response) {
            case REQUEST_LOST_PACKETS_RETURN_UPDATED_BIT_ARRAY:
                printf("got updated bit array\n");
                break;
            case REQUEST_LOST_PACKETS_RETURN_COMPLETED_PACKET:
                printf("returned completed packet\n");
                return;
        }
    
        for(uint32_t i = 0; i < packet_sending->lost_chunks_size; i++) {
            const uint32_t lost_chunk_index = packet_sending->lost_chunks[i];
            printf("index: %d\n", packet_sending->lost_chunks[i]);
    
            memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, chunk_index)], &lost_chunk_index, sizeof(lost_chunk_index));
    
            const unsigned int current_offset = lost_chunk_index * (maximum_transmission_unit - sizeof(SwiftNetPacketInfo));

            const unsigned int bytes_to_complete = (current_offset + maximum_transmission_unit - sizeof(SwiftNetPacketInfo)) - packet_length;

            if(bytes_to_complete < maximum_transmission_unit - sizeof(SwiftNetPacketInfo)) {
                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, packet_length)], &bytes_to_complete, sizeof(bytes_to_complete));

                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, chunk_size)], &bytes_to_complete, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_size));
                
                memcpy(&resend_chunk_buffer[sizeof(SwiftNetPacketInfo)], &packet->packet_data_start[current_offset], bytes_to_complete);
                
                memset(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], 0x00, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));

                uint32_t checksum = crc32(resend_chunk_buffer, bytes_to_complete);

                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));
    
                sendto(sockfd, resend_chunk_buffer, bytes_to_complete, 0, destination_address, socklen);
            } else {
                memcpy(&resend_chunk_buffer[sizeof(SwiftNetPacketInfo)], &packet->packet_data_start[current_offset], maximum_transmission_unit - sizeof(SwiftNetPacketInfo));

                memset(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], 0x00, SIZEOF_FIELD(SwiftNetPacketInfo, checksum));

                uint32_t checksum = crc32(resend_chunk_buffer, maximum_transmission_unit);

                memcpy(&resend_chunk_buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));
    
                sendto(sockfd, resend_chunk_buffer, maximum_transmission_unit, 0, destination_address, socklen);
            }
        }
    }
}

void swiftnet_send_packet(const CONNECTION_TYPE* restrict const connection EXTRA_SERVER_ARG(const SwiftNetClientAddrData client_address)) {
    printf("sending packet\n");

    SwiftNetErrorCheck(
        null_check_connection(connection);
    )

    SwiftNetClientCode(
        const unsigned int target_maximum_transmission_unit = connection->maximum_transmission_unit;

        const SwiftNetPortInfo port_info = connection->port_info;

        const volatile SwiftNetPacket* const packet = &connection->packet;

        const unsigned int packet_length = packet->packet_append_pointer - packet->packet_data_start;

        const struct sockaddr_in* target_addr = &connection->server_addr;
    )

    SwiftNetServerCode(
        const unsigned int target_maximum_transmission_unit = client_address.maximum_transmission_unit;

        SwiftNetPortInfo port_info;
        port_info.destination_port = client_address.client_address.sin_port;
        port_info.source_port = connection->server_port;

        const volatile SwiftNetPacket* const packet = &connection->packet;

        const unsigned int packet_length = packet->packet_append_pointer - packet->packet_data_start;

        const struct sockaddr_in* restrict const target_addr = &client_address.client_address;
    )

    const uint16_t packet_id = rand();

    const unsigned int mtu = MIN(target_maximum_transmission_unit, maximum_transmission_unit);

    SwiftNetPacketInfo packet_info = {
        .port_info = port_info,
        .packet_length = packet_length,
        .packet_id = packet_id,
        .chunk_size = mtu - sizeof(SwiftNetPacketInfo),
        .checksum = 0x00,
        .maximum_transmission_unit = maximum_transmission_unit
    };

    memcpy(packet->packet_buffer_start, &packet_info, sizeof(SwiftNetPacketInfo));

    if(packet_length > mtu) {
        volatile SwiftNetPacketSending* const empty_packet_sending = get_empty_packet_sending((volatile SwiftNetPacketSending* const)connection->packets_sending, MAX_PACKETS_SENDING);
        if(unlikely(empty_packet_sending == NULL)) {
            fprintf(stderr, "Failed to send a packet: exceeded maximum amount of sending packets at the same time\n");
            return;
        }

        const unsigned int chunk_amount = (packet_length + (mtu - sizeof(SwiftNetPacketInfo)) - 1) / (mtu - sizeof(SwiftNetPacketInfo));
    
        empty_packet_sending->packet_id = packet_id;

        packet_info.chunk_amount = chunk_amount;

        uint8_t buffer[mtu];

        memcpy(buffer, &packet_info, sizeof(SwiftNetPacketInfo));

        printf("after memcpy\n");

        for (unsigned int i = offsetof(SwiftNetPacketInfo, checksum); i < sizeof(SwiftNetPacketInfo); i++) {
            printf("%d ", buffer[i]);
        }
        printf("\n");

        for(unsigned int i = 0; ; i++) {
            const unsigned int current_offset = i * (mtu - sizeof(SwiftNetPacketInfo));

            memcpy(&buffer[offsetof(SwiftNetPacketInfo, chunk_index)], &i, sizeof(packet_info.chunk_index));
            
            memset(&buffer[offsetof(SwiftNetPacketInfo, checksum)], 0x00, sizeof(uint32_t));
        
            if(current_offset + mtu > packet_info.packet_length) {
                // last chunk
                const unsigned int bytes_to_send = packet_length - current_offset;

                memcpy(&buffer[sizeof(SwiftNetPacketInfo)], packet->packet_data_start + current_offset, bytes_to_send);
                memcpy(&buffer[offsetof(SwiftNetPacketInfo, chunk_size)], &bytes_to_send, sizeof(bytes_to_send));

                uint32_t checksum = crc32(buffer, bytes_to_send + sizeof(SwiftNetPacketInfo));
                memcpy(&buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));

                sendto(connection->sockfd, buffer, bytes_to_send + sizeof(SwiftNetPacketInfo), 0x00, (const struct sockaddr *)target_addr, sizeof(*target_addr));

                handle_lost_packets(empty_packet_sending, mtu, connection EXTRA_SERVER_ARG(&client_address));
                
                break;
            } else {
                for (unsigned int i = 0; i < sizeof(SwiftNetPacketInfo); i++) {
                    printf("%d ", buffer[i]);
                }
                printf("\n");

                memcpy(&buffer[sizeof(SwiftNetPacketInfo)], packet->packet_data_start + current_offset, mtu - sizeof(SwiftNetPacketInfo));

                uint32_t checksum = crc32(buffer, sizeof(buffer));

                memcpy(&buffer[offsetof(SwiftNetPacketInfo, checksum)], &checksum, sizeof(checksum));

                sendto(connection->sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
            }

            printf("sent one chunk\n");
        }
    } else {
        const uint32_t chunk_amount = 1;
        const uint32_t chunk_index = 0;

        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, chunk_size), &packet_length, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_size));
        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, chunk_amount), &chunk_amount, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_amount));
        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, chunk_index), &chunk_index, SIZEOF_FIELD(SwiftNetPacketInfo, chunk_index));

        uint32_t checksum = crc32(packet->packet_buffer_start, packet_length + sizeof(SwiftNetPacketInfo));
        memcpy(packet->packet_buffer_start + offsetof(SwiftNetPacketInfo, checksum), &checksum, sizeof(checksum));

        sendto(connection->sockfd, packet->packet_buffer_start, packet_length + sizeof(SwiftNetPacketInfo), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
    }
}
