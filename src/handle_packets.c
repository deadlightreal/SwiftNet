#include "swift_net.h"
#include <arpa/inet.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include "internal/internal.h"
#include <stddef.h>

static inline void insert_queue_node(PacketQueueNode* const new_node, volatile PacketQueue* const packet_queue, const ConnectionType contype) {
    if(new_node == NULL) {
        return;
    }

    uint32_t owner_none = PACKET_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong_explicit(&packet_queue->owner, &owner_none, PACKET_QUEUE_OWNER_HANDLE_PACKETS, memory_order_acquire, memory_order_relaxed)) {
        owner_none = PACKET_QUEUE_OWNER_NONE;
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

    atomic_store_explicit(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE, memory_order_release);

    return;
}

static inline void swiftnet_handle_packets(const uint16_t source_port, pthread_t* const process_packets_thread, void* connection, const ConnectionType connection_type, PacketQueue* const packet_queue, const _Atomic bool* closing, const bool loopback, const uint16_t addr_type, const struct pcap_pkthdr* hdr, const uint8_t* packet) {
    PacketQueueNode *node = allocator_allocate(&packet_queue_node_memory_allocator);
    if (unlikely(node == NULL)) {
        return;
    }

    uint8_t *packet_buffer = allocator_allocate(&packet_buffer_memory_allocator);
    if (unlikely(packet_buffer == NULL)) {
        allocator_free(&packet_queue_node_memory_allocator, node);
        return;
    }

    uint32_t len = hdr->caplen;
    memcpy(packet_buffer, packet, len);

    if (len == 0) {
        allocator_free(&packet_queue_node_memory_allocator, node);
        allocator_free(&packet_buffer_memory_allocator, packet_buffer);
        return;
    }

    if (addr_type == DLT_EN10MB) {
        struct ether_header *eth = (struct ether_header *)packet_buffer;

        if (ntohs(eth->ether_type) == ETHERTYPE_IP) {
            struct ip *ip_header = (struct ip *)(packet_buffer + sizeof(struct ether_header));

            node->sender_address = ip_header->ip_src;
        } else {
            allocator_free(&packet_queue_node_memory_allocator, node);
            allocator_free(&packet_buffer_memory_allocator, packet_buffer);
            return;
        }
    }

    node->data_read = len;
    node->data = packet_buffer;
    node->next = NULL;

    node->server_address_length = sizeof(node->sender_address);

    atomic_thread_fence(memory_order_release);

    insert_queue_node(node, packet_queue, connection_type);
}

static void handle_client_init(SwiftNetClientConnection* user, const struct pcap_pkthdr* hdr, const uint8_t* buffer) {
    SwiftNetClientConnection* const client_connection = (SwiftNetClientConnection*)user;

    if (atomic_load_explicit(&client_connection->closing, memory_order_acquire) == true) {
        return;
    }

    const uint32_t bytes_received = hdr->caplen;

    if(bytes_received != PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation) + client_connection->prepend_size) {
        #ifdef SWIFT_NET_DEBUG
            if (check_debug_flag(DEBUG_INITIALIZATION)) {
                send_debug_message("Invalid packet received from server. Expected server information: {\"bytes_received\": %u, \"expected_bytes\": %u}\n", bytes_received, PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation));
            }
        #endif

        return;
    }

    struct ip* ip_header = (struct ip*)(buffer + client_connection->prepend_size);
    SwiftNetPacketInfo* packet_info = (SwiftNetPacketInfo*)(buffer + client_connection->prepend_size + sizeof(struct ip));
    SwiftNetServerInformation* server_information = (SwiftNetServerInformation*)(buffer + client_connection->prepend_size + sizeof(struct ip) + sizeof(SwiftNetPacketInfo));

    if(packet_info->port_info.destination_port != client_connection->port_info.source_port || packet_info->port_info.source_port != client_connection->port_info.destination_port) {
        #ifdef SWIFT_NET_DEBUG
            if (check_debug_flag(DEBUG_INITIALIZATION)) {
                send_debug_message("Port info does not match: {\"destination_port\": %d, \"source_port\": %d, \"source_ip_address\": \"%s\"}\n", packet_info->port_info.destination_port, packet_info->port_info.source_port, inet_ntoa(ip_header->ip_src));
            }
        #endif

        return;
    }

    if(packet_info->packet_type != PACKET_TYPE_REQUEST_INFORMATION) {
        #ifdef SWIFT_NET_DEBUG
            if (check_debug_flag(DEBUG_INITIALIZATION)) {
                send_debug_message("Invalid packet type: {\"packet_type\": %d}\n", packet_info->packet_type);
            }
        #endif
        return;
    }
        
    client_connection->maximum_transmission_unit = server_information->maximum_transmission_unit;

    atomic_store_explicit(&client_connection->initialized, true, memory_order_release);
}

static void pcap_packet_handle(uint8_t* user, const struct pcap_pkthdr* hdr, const uint8_t* packet) {
    Listener* const listener = (Listener*)user;

    SwiftNetPortInfo* const port_info = (SwiftNetPortInfo*)(packet + PACKET_PREPEND_SIZE(listener->addr_type) + sizeof(struct ip) + offsetof(SwiftNetPacketInfo, port_info));

    vector_lock(&listener->servers);

    for (uint16_t i = 0; i < listener->servers.size; i++) {
        SwiftNetServer* const server = vector_get(&listener->servers, i);
        if (server->server_port == port_info->destination_port) {
            vector_unlock(&listener->servers);

            swiftnet_handle_packets(server->server_port, &server->process_packets_thread, server, CONNECTION_TYPE_SERVER, &server->packet_queue, &server->closing, server->loopback, server->addr_type, hdr, packet);

            return;
        }
    }

    vector_unlock(&listener->servers);

    vector_lock(&listener->client_connections);

    for (uint16_t i = 0; i < listener->client_connections.size; i++) {
        SwiftNetClientConnection* const client_connection = vector_get(&listener->client_connections, i);
        if (client_connection->port_info.source_port == port_info->destination_port) {
            vector_unlock(&listener->client_connections);

            if (client_connection->initialized == false) {
                handle_client_init(client_connection, hdr, packet);
            } else {
                swiftnet_handle_packets(client_connection->port_info.source_port, &client_connection->process_packets_thread, client_connection, CONNECTION_TYPE_CLIENT, &client_connection->packet_queue, &client_connection->closing, client_connection->loopback, client_connection->addr_type, hdr, packet);
            }

            return;
        }
    }

    vector_unlock(&listener->client_connections);
}

void* interface_start_listening(void* listener_void) {
    Listener* listener = listener_void;

    pcap_loop(listener->pcap, 0, pcap_packet_handle, listener_void);

    return NULL;
}
