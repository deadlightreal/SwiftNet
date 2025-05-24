#include "swift_net.h"
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include "internal/internal.h"

static inline void insert_queue_node(PacketQueueNode* const restrict new_node) {
    unsigned int owner_none = PACKET_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong(&packet_queue.owner, &owner_none, PACKET_QUEUE_OWNER_HANDLE_PACKETS)) {}

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
    const unsigned int header_size = sizeof(SwiftNetPacketInfo) + sizeof(struct ip);

    SwiftNetServerCode(
        SwiftNetServer* const volatile server = (SwiftNetServer*)void_connection;

        const unsigned min_mtu = maximum_transmission_unit;
        const int sockfd = server->sockfd;
        const uint16_t source_port = server->server_port;

        pthread_t* restrict const process_packets_thread = &server->process_packets_thread;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* const volatile connection = (SwiftNetClientConnection*)void_connection;

        unsigned int min_mtu = MIN(maximum_transmission_unit, connection->maximum_transmission_unit);
        const int sockfd = connection->sockfd;
        const uint16_t source_port = connection->port_info.source_port;

        pthread_t* restrict const process_packets_thread = &connection->process_packets_thread;
    )

    const unsigned int total_buffer_size = sizeof(struct ip) + min_mtu;

    packet_queue.first_node = NULL;
    packet_queue.last_node = NULL;

    pthread_create(process_packets_thread, NULL, process_packets, void_connection);

    while(1) {
        PacketQueueNode* const restrict node = malloc(sizeof(PacketQueueNode));
        
        node->sender_address_len = sizeof(node->sender_address);

        uint8_t* const restrict packet_buffer = malloc(total_buffer_size);
        if(packet_buffer == NULL) {
            free(node);
            continue;
        }

        const int received_sucessfully = recvfrom(sockfd, packet_buffer, total_buffer_size, 0, (struct sockaddr *)&node->sender_address, &node->sender_address_len);
        if(received_sucessfully < 0) {
            free(node);
            free(packet_buffer);
            continue;
        }

        printf("got packet %d\n", received_sucessfully);

        node->data = packet_buffer;
        node->next = NULL;

        insert_queue_node(node);
    }

    return NULL;
}
