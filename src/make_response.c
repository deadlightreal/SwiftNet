#include "internal/internal.h"
#include "swift_net.h"

#ifndef SWIFT_NET_DISABLE_REQUESTS

void swiftnet_client_make_response(struct SwiftNetClientConnection* const client, struct SwiftNetClientPacketData* const packet_data, struct SwiftNetPacketBuffer* restrict const buffer) {
    #ifdef SWIFT_NET_BACKEND_DPDK
    const uint32_t packet_length = buffer->total_data_size;
    #else
    const uint32_t packet_length = (uint32_t)(buffer->packet_append_pointer - buffer->packet_data_start);
    #endif

    swiftnet_send_packet(client->maximum_transmission_unit, client->port_info, buffer, packet_length, &client->server_addr, &client->packets_sending, &client->packets_sending_memory_allocator, client->eth_header, client->network_data, NULL, true, packet_data->metadata.packet_id);
}

void swiftnet_server_make_response(struct SwiftNetServer* const server, struct SwiftNetServerPacketData* const packet_data, struct SwiftNetPacketBuffer* restrict const buffer) {
    #ifdef SWIFT_NET_BACKEND_DPDK
    const uint32_t packet_length = buffer->total_data_size;
    #else
    const uint32_t packet_length = (uint32_t)(buffer->packet_append_pointer - buffer->packet_data_start);
    #endif
    const struct SwiftNetPortInfo port_info = {.source_port = server->server_port, .destination_port = packet_data->metadata.port_info.source_port};

    swiftnet_send_packet(packet_data->metadata.sender.maximum_transmission_unit, port_info, buffer, packet_length, &packet_data->metadata.sender.sender_address, &server->packets_sending, &server->packets_sending_memory_allocator, server->eth_header, server->network_data, NULL, true, packet_data->metadata.packet_id);
}

#endif
