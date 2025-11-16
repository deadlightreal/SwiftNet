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

static inline void swiftnet_handle_packets(pcap_t* const pcap, const uint16_t source_port, pthread_t* const process_packets_thread, void* connection, const ConnectionType connection_type, PacketQueue* const packet_queue, const _Atomic bool* closing, const bool loopback) {
    struct pcap_pkthdr *hdr;
    const uint8_t* pkt_data;

    while (1) {
        int r = pcap_next_ex(pcap, &hdr, &pkt_data);

        if (r == 0) {
            usleep(1000);
            continue;
        }
        if (r == -1) {
            fprintf(stderr, "pcap error: %s\n", pcap_geterr(pcap));
            break;
        }
        if (r == -2) {
            break;
        }

        printf("got packet: %u bytes\n", hdr->caplen);

        PacketQueueNode *node = allocator_allocate(&packet_queue_node_memory_allocator);
        if (unlikely(node == NULL))
            continue;

        uint8_t *packet_buffer = allocator_allocate(&packet_buffer_memory_allocator);
        if (unlikely(packet_buffer == NULL)) {
            allocator_free(&packet_queue_node_memory_allocator, node);
            continue;
        }

        uint32_t len = hdr->caplen;
        memcpy(packet_buffer, pkt_data, len);

        if (len == 0) {
            allocator_free(&packet_queue_node_memory_allocator, node);
            allocator_free(&packet_buffer_memory_allocator, packet_buffer);
            continue;
        }

        if (!loopback) {
            struct ether_header *eth = (struct ether_header *)packet_buffer;

            if (ntohs(eth->ether_type) == ETHERTYPE_IP) {
                struct ip *ip_header = (struct ip *)(packet_buffer + sizeof(struct ether_header));

                node->sender_address.sin_addr = ip_header->ip_src;
            } else {
                allocator_free(&packet_queue_node_memory_allocator, node);
                allocator_free(&packet_buffer_memory_allocator, packet_buffer);
                continue;
            }
        }

        node->data_read = len;
        node->data = packet_buffer;
        node->next = NULL;

        node->server_address_length = sizeof(node->sender_address);

        atomic_thread_fence(memory_order_release);

        insert_queue_node(node, packet_queue, connection_type);
    }
}


void* swiftnet_client_handle_packets(void* const client_void) {
    SwiftNetClientConnection* const client = (SwiftNetClientConnection*)client_void;

    swiftnet_handle_packets(client->pcap, client->port_info.source_port, &client->process_packets_thread, client, CONNECTION_TYPE_CLIENT, &client->packet_queue, &client->closing, client->loopback);

    return NULL;
}

void* swiftnet_server_handle_packets(void* const server_void) {
    SwiftNetServer* const server = (SwiftNetServer*)server_void;

    swiftnet_handle_packets(server->pcap, server->server_port, &server->process_packets_thread, server, CONNECTION_TYPE_SERVER, &server->packet_queue, &server->closing, server->loopback);

    return NULL;
}
