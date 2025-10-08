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

static inline void insert_queue_node(PacketQueueNode* const restrict new_node, PacketQueue* restrict const packet_queue, const ConnectionType contype) {
    if(new_node == NULL) {
        return;
    }

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

static inline void swiftnet_handle_packets(const int sockfd, const uint16_t source_port, pthread_t* restrict const process_packets_thread, void* connection, const ConnectionType connection_type, PacketQueue* restrict const packet_queue) {
    if(connection_type == CONNECTION_TYPE_CLIENT) {
        pthread_create(process_packets_thread, NULL, swiftnet_client_process_packets, connection);
    } else {
        pthread_create(process_packets_thread, NULL, swiftnet_server_process_packets, connection);
    }

    while(1) {
        PacketQueueNode* const restrict node = allocator_allocate(&packet_queue_node_memory_allocator);
        if(unlikely(node == NULL)) {
            continue;
        }

        node->server_address_length = sizeof(node->sender_address);

        uint8_t* const restrict packet_buffer = malloc(maximum_transmission_unit);
        if(unlikely(packet_buffer == NULL)) {
            allocator_free(&packet_queue_node_memory_allocator, node);
            continue;
        }

        const int received_sucessfully = recvfrom(sockfd, packet_buffer, maximum_transmission_unit, 0, (struct sockaddr *)&node->sender_address, &node->server_address_length);
        
        if(received_sucessfully < 0) {
            allocator_free(&packet_queue_node_memory_allocator, node);
            free(packet_buffer);
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

void* swiftnet_client_handle_packets(void* restrict const client_void) {
    SwiftNetClientConnection* restrict const client = (SwiftNetClientConnection*)client_void;

    swiftnet_handle_packets(client->sockfd, client->port_info.source_port, &client->process_packets_thread, client, CONNECTION_TYPE_CLIENT, &client->packet_queue);

    return NULL;
}

void* swiftnet_server_handle_packets(void* restrict const server_void) {
    SwiftNetServer* restrict const server = (SwiftNetServer*)server_void;

    swiftnet_handle_packets(server->sockfd, server->server_port, &server->process_packets_thread, server, CONNECTION_TYPE_SERVER, &server->packet_queue);

    return NULL;
}
