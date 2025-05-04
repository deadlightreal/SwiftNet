#include "swift_net.h"
#include <stdarg.h>
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

static inline void null_check_connection(void* ptr) {
    if(unlikely(ptr == NULL)) {
        fprintf(stderr, "Error: First argument ( Client | Server ) in send packet function is NULL!!\n");
        exit(EXIT_FAILURE);
    }
}

static inline SwiftNetPacketSending* get_empty_packet_sending(SwiftNetPacketSending* packet_sending_array, const uint16_t size) {
    for(uint16_t i = 0; i < size; i++) {
        SwiftNetPacketSending* current_packet_sending = &packet_sending_array[i];

        if(current_packet_sending->packet_id == 0x00) {
            return current_packet_sending;
        }
    }

    return NULL;
}

static inline void wait_for_next_chunk_request(bool* requested_next_chunk) {
    while(*requested_next_chunk == false) {
    }

    *requested_next_chunk = false;
}

void swiftnet_send_packet(CONNECTION_TYPE* connection SEND_PACKET_EXTRA_ARG) {
    printf("sending packet\n");

    SwiftNetErrorCheck(
        null_check_connection(connection);
    )

    SwiftNetClientCode(
        const unsigned int target_maximum_transmission_unit = connection->maximum_transmission_unit;

        SwiftNetPortInfo port_info = connection->port_info;

        SwiftNetPacket* packet = &connection->packet;

        const unsigned int packet_length = packet->packet_append_pointer - packet->packet_data_start;

        const struct sockaddr_in* target_addr = &connection->server_addr;
    )

    SwiftNetServerCode(
        const unsigned int target_maximum_transmission_unit = client_address.maximum_transmission_unit;

        SwiftNetPortInfo port_info;
        port_info.destination_port = client_address.client_address.sin_port;
        port_info.source_port = connection->server_port;

        SwiftNetPacket* packet = &connection->packet;

        const unsigned int packet_length = packet->packet_append_pointer - packet->packet_data_start;

        const struct sockaddr_in* target_addr = &client_address.client_address;
    )

    uint16_t packet_id = rand();

    unsigned int mtu = MIN(target_maximum_transmission_unit, maximum_transmission_unit);

    SwiftNetPacketInfo packet_info;
    packet_info.port_info = port_info;
    packet_info.packet_length = packet_length;
    packet_info.packet_id = packet_id;
    packet_info.chunk_size = mtu;
    packet_info.packet_type = PACKET_TYPE_MESSAGE;

    memcpy(packet->packet_buffer_start, &packet_info, sizeof(SwiftNetPacketInfo));

    if(packet_length > mtu) {
        SwiftNetPacketSending* empty_packet_sending = get_empty_packet_sending(connection->packets_sending, MAX_PACKETS_SENDING);
        if(unlikely(empty_packet_sending == NULL)) {
            fprintf(stderr, "Failed to send a packet: exceeded maximum amount of sending packets at the same time\n");
            return;
        }
    
        empty_packet_sending->packet_id = packet_id;
        empty_packet_sending->requested_next_chunk = false;

        uint8_t buffer[mtu];

        memcpy(buffer, &packet_info, sizeof(SwiftNetPacketInfo));

        for(unsigned int current_offset = 0; ; current_offset += mtu - sizeof(SwiftNetPacketInfo)) {
            if(current_offset + mtu > packet_info.packet_length) {
                // last chunk
                unsigned int bytes_to_send = packet_length - current_offset;

                memcpy(&buffer[sizeof(SwiftNetPacketInfo)], packet->packet_data_start + current_offset, bytes_to_send);
                sendto(connection->sockfd, buffer, bytes_to_send + sizeof(SwiftNetPacketInfo), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));

                memset(empty_packet_sending, 0, sizeof(SwiftNetPacketSending));
                
                break;
            } else {
                memcpy(&buffer[sizeof(SwiftNetPacketInfo)], packet->packet_data_start + current_offset, sizeof(buffer));
                sendto(connection->sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
            }

            wait_for_next_chunk_request(&empty_packet_sending->requested_next_chunk);
        }
    } else {
        memcpy(packet->packet_buffer_start, &packet_info, sizeof(SwiftNetPacketInfo));
        
        sendto(connection->sockfd, packet->packet_buffer_start, packet_length + sizeof(SwiftNetPacketInfo), 0, (const struct sockaddr *)target_addr, sizeof(*target_addr));
    }
}
