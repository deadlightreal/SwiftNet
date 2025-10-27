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

static inline void insert_queue_node(PacketQueueNode* const new_node, PacketQueue* const packet_queue, const ConnectionType contype) {
    if(new_node == NULL) {
        printf("null node\n");

        return;
    }

    printf("inserted\n");

    uint32_t owner_none = PACKET_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong(&packet_queue->owner, &owner_none, PACKET_QUEUE_OWNER_HANDLE_PACKETS)) {
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

    atomic_store(&packet_queue->owner, PACKET_QUEUE_OWNER_NONE);

    return;
}

static inline void swiftnet_handle_packets(const int sockfd, const uint16_t source_port, pthread_t* const process_packets_thread, void* connection, const ConnectionType connection_type, PacketQueue* const packet_queue, const _Atomic bool* closing) {
    while(1) {
        if (atomic_load(closing) == true) {
            break;
        }

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

        const int received_sucessfully = recvfrom(sockfd, packet_buffer, maximum_transmission_unit, 0, (struct sockaddr *)&node->sender_address, &node->server_address_length);

        printf("got packet\n");
        
        if(received_sucessfully < 0) {
            printf("receiving failed\n");

            allocator_free(&packet_queue_node_memory_allocator, node);
            allocator_free(&packet_buffer_memory_allocator, packet_buffer);
            continue;
        }

        struct in_addr sender_addr;
        memcpy(&sender_addr, &packet_buffer[offsetof(struct ip, ip_src)], sizeof(struct in_addr));

        node->data_read = received_sucessfully;
        node->data = packet_buffer;
        node->sender_address.sin_addr = sender_addr;
        node->next = NULL;

        insert_queue_node(node, packet_queue, connection_type);
    }
}

void* swiftnet_client_handle_packets(void* const client_void) {
    SwiftNetClientConnection* const client = (SwiftNetClientConnection*)client_void;

    printf("creating handle packets thread\n");

    swiftnet_handle_packets(client->sockfd, client->port_info.source_port, &client->process_packets_thread, client, CONNECTION_TYPE_CLIENT, &client->packet_queue, &client->closing);

    return NULL;
}

void* swiftnet_server_handle_packets(void* const server_void) {
    SwiftNetServer* const server = (SwiftNetServer*)server_void;

    printf("creating handle packets thread\n");

    swiftnet_handle_packets(server->sockfd, server->server_port, &server->process_packets_thread, server, CONNECTION_TYPE_SERVER, &server->packet_queue, &server->closing);

    return NULL;
}
