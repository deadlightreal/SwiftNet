#include "swift_net.h"
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include "internal/internal.h"

static inline void insert_queue_node(PacketQueueNode* new_node) {
    printf("inserting node\n");

    if(packet_queue.last_node == NULL) {
        packet_queue.last_node = new_node;
        packet_queue.first_node = new_node;
    } else {
        packet_queue.last_node->next = new_node;

        packet_queue.last_node = new_node;
    }
}

void* swiftnet_handle_packets(void* void_connection) {
    const unsigned int header_size = sizeof(SwiftNetPacketInfo) + sizeof(struct ip);

    SwiftNetServerCode(
        SwiftNetServer* server = (SwiftNetServer*)void_connection;

        void** packet_handler = (void**)&server->packet_handler;

        const unsigned min_mtu = maximum_transmission_unit;
        const int sockfd = server->sockfd;
        const uint16_t source_port = server->server_port;
        const unsigned int* buffer_size = &server->buffer_size;
        SwiftNetPacketSending* packet_sending = server->packets_sending;
        const uint16_t packet_sending_size = MAX_PACKETS_SENDING;

        pthread_t* process_packets_thread = &server->process_packets_thread;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* connection = (SwiftNetClientConnection*)void_connection;

        void** packet_handler = (void**)&connection->packet_handler;

        unsigned int min_mtu = MIN(maximum_transmission_unit, connection->maximum_transmission_unit);
        const int sockfd = connection->sockfd;
        const uint16_t source_port = connection->port_info.source_port;
        const unsigned int* buffer_size = &connection->buffer_size;
        SwiftNetPacketSending* packet_sending = connection->packets_sending;
        const uint16_t packet_sending_size = MAX_PACKETS_SENDING;

        pthread_t* process_packets_thread = &connection->process_packets_thread;
    )

    const unsigned int total_buffer_size = header_size + min_mtu;

    packet_queue.first_node = NULL;
    packet_queue.last_node = NULL;

    pthread_create(process_packets_thread, NULL, process_packets, void_connection);

    while(1) {
        PacketQueueNode* node = malloc(sizeof(PacketQueueNode));
        
        node->sender_address_len = sizeof(node->sender_address);

        uint8_t* packet_buffer = malloc(total_buffer_size);

        printf("doing recvfrom\n");

        recvfrom(sockfd, packet_buffer, total_buffer_size, 0, (struct sockaddr *)&node->sender_address, &node->sender_address_len);

        printf("after recvfrom\n");

        node->data = packet_buffer;
        node->next = NULL;

        insert_queue_node(node);
    }

    return NULL;
}
