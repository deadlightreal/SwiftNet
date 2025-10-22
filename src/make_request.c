#include "internal/internal.h"
#include "swift_net.h"
#include <stdlib.h>
#include <unistd.h>

#ifdef SWIFT_NET_REQUESTS

SwiftNetClientPacketData* swiftnet_client_make_request(SwiftNetClientConnection* restrict const client, SwiftNetPacketBuffer* restrict const packet) {
    volatile RequestSent* const request_sent = allocator_allocate(&requests_sent_memory_allocator);
    request_sent->packet_data = NULL;
    request_sent->address = client->server_addr.sin_addr.s_addr;

    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;

    swiftnet_send_packet(client, client->maximum_transmission_unit, client->port_info, packet, packet_length, &client->server_addr, &client->server_addr_len, &client->packets_sending, &client->packets_sending_memory_allocator, client->sockfd, request_sent, false, 0);

    while (1) {
        if (request_sent->packet_data != NULL) {
            SwiftNetClientPacketData* packet_data = request_sent->packet_data;

            return packet_data;
        }

        usleep(5000);
    }
}

SwiftNetServerPacketData* swiftnet_server_make_request(SwiftNetServer* restrict const server, SwiftNetPacketBuffer* restrict const packet, const SwiftNetClientAddrData addr_data) {
    volatile RequestSent* const request_sent = allocator_allocate(&requests_sent_memory_allocator);
    request_sent->packet_data = NULL;
    request_sent->address = addr_data.sender_address.sin_addr.s_addr;

    const uint32_t packet_length = packet->packet_append_pointer - packet->packet_data_start;

    const SwiftNetPortInfo port_info = {
        .destination_port = addr_data.sender_address.sin_port,
        .source_port = server->server_port
    };

    swiftnet_send_packet(server, addr_data.maximum_transmission_unit, port_info, packet, packet_length, &addr_data.sender_address, &addr_data.sender_address_length, &server->packets_sending, &server->packets_sending_memory_allocator, server->sockfd, request_sent, false, 0);

    while (1) {
        if (request_sent->packet_data != NULL) {
            SwiftNetServerPacketData* packet_data = request_sent->packet_data;

            return packet_data;
        }

        usleep(5000);
    }
}

#endif
