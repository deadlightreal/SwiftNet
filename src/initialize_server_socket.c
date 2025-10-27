#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>
#include "internal/internal.h"
#include "swift_net.h"

SwiftNetServer* swiftnet_create_server(const uint16_t port) {
    SwiftNetServer* const new_server = allocator_allocate(&server_memory_allocator);

    #ifdef SWIFT_NET_ERROR
        if(unlikely(new_server == NULL)) {
            fprintf(stderr, "Failed to get an empty server\n");
            exit(EXIT_FAILURE);
        }
    #endif

    new_server->server_port = port;

    // Create the socket
    new_server->sockfd = socket(AF_INET, SOCK_RAW, PROTOCOL_NUMBER);
    if (unlikely(new_server->sockfd < 0)) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    int on = 1;
    if(setsockopt(new_server->sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        fprintf(stderr, "Failed to set sockopt IP_HDRINCL\n");
        exit(EXIT_FAILURE);
    }

    new_server->packet_queue = (PacketQueue){
        .first_node = NULL,
        .last_node = NULL
    };

    atomic_store(&new_server->packet_queue.owner, PACKET_QUEUE_OWNER_NONE);

    memset(&new_server->packet_callback_queue, 0x00, sizeof(PacketCallbackQueue));
    atomic_store(&new_server->packet_callback_queue.owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

    atomic_store(&new_server->packet_handler, NULL);

    new_server->pending_messages_memory_allocator = allocator_create(sizeof(SwiftNetPendingMessage), 100);
    new_server->pending_messages = vector_create(100);
    new_server->packets_sending_memory_allocator = allocator_create(sizeof(SwiftNetPacketSending), 100);
    new_server->packets_sending = vector_create(100);
    new_server->packets_completed_memory_allocator = allocator_create(sizeof(SwiftNetPacketCompleted), 100);
    new_server->packets_completed = vector_create(100);

    new_server->closing = false;

    // Create a new thread that will handle all packets received
    pthread_create(&new_server->handle_packets_thread, NULL, swiftnet_server_handle_packets, new_server);
    pthread_create(&new_server->process_packets_thread, NULL, swiftnet_server_process_packets, new_server);
    pthread_create(&new_server->execute_callback_thread, NULL, execute_packet_callback_server, new_server);

    #ifdef SWIFT_NET_DEBUG
        if (check_debug_flag(DEBUG_INITIALIZATION)) {
            send_debug_message("Successfully initialized server\n");
        }
    #endif

    return new_server;
}
