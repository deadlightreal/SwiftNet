#include "swift_net.h"
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include "internal/internal.h"

static inline void insert_queue_node(PacketQueueNode* const restrict new_node) {
    if(new_node == NULL) {
        return;
    }

    uint32_t owner_none = PACKET_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong(&packet_queue.owner, &owner_none, PACKET_QUEUE_OWNER_HANDLE_PACKETS)) {
        owner_none = PACKET_QUEUE_OWNER_NONE;
    }

    if(packet_queue.last_node == NULL) {
        packet_queue.last_node = new_node;
    } else {
        packet_queue.last_node->next = new_node;

        packet_queue.last_node = new_node;
    }

    if(packet_queue.first_node == NULL) {
        packet_queue.first_node = new_node;
    }

    atomic_store(&packet_queue.owner, PACKET_QUEUE_OWNER_NONE);

    return;
}

void* swiftnet_handle_packets(void* const volatile void_connection) {
    SwiftNetServerCode(
        SwiftNetServer* const volatile server = (SwiftNetServer*)void_connection;

        const uint32_t mtu = maximum_transmission_unit;
        const int sockfd = server->sockfd;
        const uint16_t source_port = server->server_port;

        pthread_t* restrict const process_packets_thread = &server->process_packets_thread;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* const volatile connection = (SwiftNetClientConnection*)void_connection;

        const uint32_t mtu = MIN(maximum_transmission_unit, connection->maximum_transmission_unit);
        const int sockfd = connection->sockfd;
        const uint16_t source_port = connection->port_info.source_port;

        pthread_t* restrict const process_packets_thread = &connection->process_packets_thread;
    )

    pthread_create(process_packets_thread, NULL, process_packets, void_connection);

    while(1) {
        PacketQueueNode* const restrict node = malloc(sizeof(PacketQueueNode));
        if(unlikely(node == NULL)) {
            continue;
        }

        node->server_address_length = sizeof(node->sender_address);

        uint8_t* const restrict packet_buffer = malloc(mtu);
        if(packet_buffer == NULL) {
            free(node);
            continue;
        }

        const int received_sucessfully = recvfrom(sockfd, packet_buffer, mtu, 0, (struct sockaddr *)&node->sender_address, &node->server_address_length);
        if(received_sucessfully < 0) {
            free(node);
            free(packet_buffer);
            continue;
        }

        node->data_read = received_sucessfully;
        node->data = packet_buffer;
        node->next = NULL;

        insert_queue_node(node);
    }

    return NULL;
}
