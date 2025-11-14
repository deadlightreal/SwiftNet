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
#include <net/bpf.h>

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

static inline void swiftnet_handle_packets(const int bpf, const uint16_t source_port, pthread_t* const process_packets_thread, void* connection, const ConnectionType connection_type, PacketQueue* const packet_queue, const _Atomic bool* closing) {
    uint8_t buffer[10000];

    while(1) {
        if (atomic_load_explicit(closing, memory_order_acquire) == true) {
            break;
        }

        const int data_read = read(bpf, buffer, sizeof(buffer));
        if (data_read <= 0) {
            usleep(1000);
            continue;
        }

        uint32_t offset = 0;

        while (offset < data_read) {
            PacketQueueNode* const node = allocator_allocate(&packet_queue_node_memory_allocator);
            if(unlikely(node == NULL)) {
                continue;
            }

            node->server_address_length = sizeof(node->sender_address);

            uint8_t* const packet_buffer = allocator_allocate(&packet_buffer_memory_allocator);
            if(unlikely(packet_buffer == NULL)) {
                allocator_free(&packet_queue_node_memory_allocator, node);
                continue;
            }

            struct bpf_hdr *hdr = (struct bpf_hdr *)(buffer + offset);
            unsigned char *pkt = buffer + offset + hdr->bh_hdrlen;
            uint32_t len = hdr->bh_caplen;

            printf("got packet\n");

            memcpy(packet_buffer, pkt, len);
        
            if(len == 0) {
                allocator_free(&packet_queue_node_memory_allocator, node);
                allocator_free(&packet_buffer_memory_allocator, packet_buffer);
                continue;
            }

            struct in_addr sender_addr;
            memcpy(&sender_addr, &packet_buffer[sizeof(struct ether_header) + offsetof(struct ip, ip_src)], sizeof(struct in_addr));

            node->data_read = len;
            node->data = packet_buffer;
            node->sender_address.sin_addr = sender_addr;
            node->next = NULL;

            atomic_thread_fence(memory_order_release);

            insert_queue_node(node, packet_queue, connection_type);

            offset += BPF_WORDALIGN(hdr->bh_hdrlen + len);
        }
    }
}

void* swiftnet_client_handle_packets(void* const client_void) {
    SwiftNetClientConnection* const client = (SwiftNetClientConnection*)client_void;

    swiftnet_handle_packets(client->bpf, client->port_info.source_port, &client->process_packets_thread, client, CONNECTION_TYPE_CLIENT, &client->packet_queue, &client->closing);

    return NULL;
}

void* swiftnet_server_handle_packets(void* const server_void) {
    SwiftNetServer* const server = (SwiftNetServer*)server_void;

    swiftnet_handle_packets(server->bpf, server->server_port, &server->process_packets_thread, server, CONNECTION_TYPE_SERVER, &server->packet_queue, &server->closing);

    return NULL;
}
