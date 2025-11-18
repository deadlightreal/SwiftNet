#include "swift_net.h"
#include <arpa/inet.h>
#include <stdatomic.h>
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

static inline void swiftnet_handle_packets(const uint16_t source_port, pthread_t* const process_packets_thread, void* connection, const ConnectionType connection_type, PacketQueue* const packet_queue, const _Atomic bool* closing, const bool loopback, const struct pcap_pkthdr* hdr, const uint8_t* packet) {
    printf("got packet: %u bytes\n", hdr->caplen);

    for (uint32_t i = 0; i < hdr->caplen; i++) {
        printf("%d ", packet[i]);
    }
    printf("\n");

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

    if (!loopback) {
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

static void pcap_packet_handle(uint8_t* user, const struct pcap_pkthdr* hdr, const uint8_t* packet) {
    Listener* const listener = (Listener*)user;

    SwiftNetPortInfo* const port_info = (SwiftNetPortInfo*)(packet + PACKET_PREPEND_SIZE(listener->loopback) + sizeof(struct ip) + offsetof(SwiftNetPacketInfo, port_info));

    printf("received packet for port: %d\n", port_info->destination_port);

    vector_lock(&listener->servers);

    for (uint16_t i = 0; i < listener->servers.size; i++) {
        SwiftNetServer* const server = vector_get(&listener->servers, i);
        if (server->server_port == port_info->destination_port) {
            vector_unlock(&listener->servers);

            swiftnet_handle_packets(server->server_port, &server->process_packets_thread, server, CONNECTION_TYPE_SERVER, &server->packet_queue, &server->closing, server->loopback, hdr, packet);

            return;
        }
    }

    vector_unlock(&listener->servers);

    vector_lock(&listener->client_connections);

    for (uint16_t i = 0; i < listener->client_connections.size; i++) {
        SwiftNetClientConnection* const client_connection = vector_get(&listener->client_connections, i);
        if (client_connection->port_info.source_port == port_info->destination_port) {
            vector_unlock(&listener->client_connections);

            swiftnet_handle_packets(client_connection->port_info.source_port, &client_connection->process_packets_thread, client_connection, CONNECTION_TYPE_CLIENT, &client_connection->packet_queue, &client_connection->closing, client_connection->loopback, hdr, packet);

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
