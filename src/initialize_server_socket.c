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

static inline SwiftNetServer* get_empty_server(SwiftNetServer* const restrict servers, const uint32_t server_count) {
    for(uint16_t i = 0; i < server_count; i++) {
        SwiftNetServer* const restrict current_server = &servers[i];
        if(current_server->sockfd == -1) {
            return current_server;
        }
    }

    return NULL;
}

SwiftNetServer* swiftnet_create_server(const uint16_t port) {
    SwiftNetServer* restrict empty_server = get_empty_server(SwiftNetServers, MAX_SERVERS);
    if(unlikely(empty_server == NULL)) {
        return NULL;
    }
    
    SwiftNetErrorCheck(
        if(unlikely(empty_server == NULL)) {
            fprintf(stderr, "Failed to get an empty server\n");
            exit(EXIT_FAILURE);
        }
    )

    empty_server->server_port = port;

    // Create the socket
    empty_server->sockfd = socket(AF_INET, SOCK_RAW, PROTOCOL_NUMBER);
    if (unlikely(empty_server->sockfd < 0)) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    int on = 1;
    if(setsockopt(empty_server->sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        fprintf(stderr, "Failed to set sockopt IP_HDRINCL\n");
        exit(EXIT_FAILURE);
    }

    const uint8_t opt = 1;
    setsockopt(empty_server->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    empty_server->packet_queue = (PacketQueue){
        .first_node = NULL,
        .last_node = NULL
    };

    atomic_store(&empty_server->packet_queue.owner, PACKET_QUEUE_OWNER_NONE);

    memset(empty_server->pending_messages, 0x00, MAX_PENDING_MESSAGES * sizeof(SwiftNetPendingMessage));
    // Initialize transfer clients to NULL | 0x00

    memset((void *)empty_server->packets_sending, 0x00, MAX_PACKETS_SENDING * sizeof(SwiftNetPacketSending));
    memset((void *)empty_server->packets_sending, 0x00, MAX_SENT_SUCCESSFULLY_COMPLETED_PACKET_SIGNAL * sizeof(SwiftNetSentSuccessfullyCompletedPacketSignal));
    memset((void *)empty_server->packets_completed_history, 0x00, MAX_COMPLETED_PACKETS_HISTORY_SIZE * sizeof(SwiftNetPacketCompleted));

    memset(&empty_server->packet_callback_queue, 0x00, sizeof(PacketCallbackQueue));
    atomic_store(&empty_server->packet_callback_queue.owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

    // Create a new thread that will handle all packets received
    pthread_create(&empty_server->handle_packets_thread, NULL, swiftnet_server_handle_packets, empty_server);

    SwiftNetDebug(
        if (check_debug_flag(DEBUG_INITIALIZATION)) {
            send_debug_message("Successfully initialized server\n");
        }
    )

    return empty_server;
}
