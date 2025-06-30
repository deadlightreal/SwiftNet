#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "internal/internal.h"
#include "swift_net.h"
#include <fcntl.h>

bool exit_thread = false;

typedef struct {
    const int sockfd;
    const void* restrict const data;
    const uint32_t size;
    const struct sockaddr_in server_addr;
    const socklen_t server_addr_len;
} RequestServerInformationArgs;

void* request_server_information(void* request_server_information_args_void) {
    const RequestServerInformationArgs* restrict const request_server_information_args = (RequestServerInformationArgs*)request_server_information_args_void;

    while (1) {
        if(exit_thread == true) {
            return NULL;
        }

        sendto(request_server_information_args->sockfd, request_server_information_args->data, request_server_information_args->size, 0, (struct sockaddr *)&request_server_information_args->server_addr, request_server_information_args->server_addr_len);

        usleep(1000000);
    }

    return NULL;
}

// Create the socket, and set client and server info
SwiftNetClientConnection* swiftnet_create_client(const char* const restrict ip_address, const uint16_t port) {
    SwiftNetClientConnection* restrict empty_connection = NULL;
    for(uint8_t i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        SwiftNetClientConnection* const restrict currentConnection = &SwiftNetClientConnections[i];
        if(currentConnection->sockfd != -1) {
            continue;
        }

        empty_connection = currentConnection;

        break;
    }

    SwiftNetErrorCheck(
        if(unlikely(empty_connection == NULL)) {
            perror("Failed to get an empty connection\n");
            exit(EXIT_FAILURE);
        }
    )

    empty_connection->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(unlikely(empty_connection->sockfd < 0)) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
    
    const uint16_t clientPort = rand();

    empty_connection->packet_queue = (PacketQueue){
        .first_node = NULL,
        .last_node = NULL
    };

    atomic_store(&empty_connection->packet_queue.owner, PACKET_QUEUE_OWNER_NONE);

    empty_connection->server_addr_len = sizeof(empty_connection->server_addr);

    empty_connection->port_info.destination_port = port;
    empty_connection->port_info.source_port = clientPort;
    empty_connection->packet_handler = NULL;

    // Base buffer size
    empty_connection->buffer_size = DEFAULT_BUFFER_SIZE;

    // Allocate memory for the packet buffer
    uint8_t* restrict const buffer_pointer = malloc(empty_connection->buffer_size + sizeof(SwiftNetPacketInfo));
    if(unlikely(buffer_pointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    uint8_t* restrict const data_pointer = buffer_pointer + sizeof(SwiftNetPacketInfo);

    empty_connection->packet.packet_buffer_start = buffer_pointer;
    empty_connection->packet.packet_data_start = data_pointer;
    empty_connection->packet.packet_append_pointer = data_pointer;

    memset(&empty_connection->server_addr, 0, sizeof(struct sockaddr_in));
    empty_connection->server_addr.sin_family = AF_INET;
    empty_connection->server_addr.sin_port = htons(port);
    empty_connection->server_addr.sin_addr.s_addr = inet_addr(ip_address);

    // Request the server information, and proccess it
    SwiftNetPacketInfo request_server_information_packet_info = {
        .port_info = empty_connection->port_info,
        .packet_length = 0x00,
        .packet_id = rand(),
        .packet_type = PACKET_TYPE_REQUEST_INFORMATION,
        .chunk_size = 0x00,
        .checksum = 0x00,
        .maximum_transmission_unit = maximum_transmission_unit
    };

     request_server_information_packet_info.checksum = crc32((const uint8_t*)&request_server_information_packet_info, sizeof(SwiftNetPacketInfo));

    memset(empty_connection->pending_messages, 0x00, MAX_PENDING_MESSAGES * sizeof(SwiftNetPendingMessage));
    memset((void *)empty_connection->packets_sending, 0x00, MAX_PACKETS_SENDING * sizeof(SwiftNetPacketSending));
    memset((void *)empty_connection->packets_sending, 0x00, MAX_SENT_SUCCESSFULLY_COMPLETED_PACKET_SIGNAL * sizeof(SwiftNetSentSuccessfullyCompletedPacketSignal));
    memset((void *)empty_connection->packets_completed_history, 0x00, MAX_COMPLETED_PACKETS_HISTORY_SIZE * sizeof(SwiftNetPacketCompleted));

    memset(&empty_connection->packet_callback_queue, 0x00, sizeof(PacketCallbackQueue));
    atomic_store(&empty_connection->packet_callback_queue.owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

    uint8_t server_information_buffer[PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation)];

    pthread_t send_request_thread;

    const RequestServerInformationArgs thread_args = {
        .sockfd = empty_connection->sockfd,
        .data = &request_server_information_packet_info,
        .size = sizeof(request_server_information_packet_info),
        .server_addr = empty_connection->server_addr,
        .server_addr_len = sizeof(empty_connection->server_addr)
    };

    pthread_create(&send_request_thread, NULL, request_server_information, (void*)&thread_args);
    
    while(1) {
        const int bytes_received = recvfrom(empty_connection->sockfd, server_information_buffer, sizeof(server_information_buffer), 0x00, NULL, NULL);
        if(bytes_received != PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation)) {
            continue;
        }

        const SwiftNetPacketInfo* const restrict packetInfo = (SwiftNetPacketInfo *)&server_information_buffer[sizeof(struct ip)];

        if(packetInfo->port_info.destination_port != empty_connection->port_info.source_port || packetInfo->port_info.source_port != empty_connection->port_info.destination_port) {
            continue;
        }

        if(packetInfo->packet_type != PACKET_TYPE_REQUEST_INFORMATION) {
            continue;
        }
            
        if(bytes_received != 0) {
            break;
        }
    }

    exit_thread = true;

    pthread_join(send_request_thread, NULL);

    const SwiftNetServerInformation* const restrict server_information = (SwiftNetServerInformation*)&server_information_buffer[PACKET_HEADER_SIZE];

    empty_connection->maximum_transmission_unit = server_information->maximum_transmission_unit;
 
    pthread_create(&empty_connection->handle_packets_thread, NULL, swiftnet_client_handle_packets, empty_connection);

    return empty_connection;
}
