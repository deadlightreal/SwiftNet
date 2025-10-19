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
#include <stddef.h>

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
            exit_thread = false;

            return NULL;
        }

        SwiftNetDebug(
            if (check_debug_flag(DEBUG_INITIALIZATION)) {
                send_debug_message("Requested server information: {\"server_ip_address\": \"%s\"}\n", inet_ntoa(request_server_information_args->server_addr.sin_addr));
            }
        )

        sendto(request_server_information_args->sockfd, request_server_information_args->data, request_server_information_args->size, 0, (struct sockaddr *)&request_server_information_args->server_addr, request_server_information_args->server_addr_len);

        usleep(1000000);
    }

    return NULL;
}

// Create the socket, and set client and server info
SwiftNetClientConnection* swiftnet_create_client(const char* const restrict ip_address, const uint16_t port) {
    SwiftNetClientConnection* new_connection = allocator_allocate(&client_connection_memory_allocator);

    new_connection->sockfd = socket(AF_INET, SOCK_RAW, PROTOCOL_NUMBER);
    if(unlikely(new_connection->sockfd < 0)) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    int on = 1;
    if(setsockopt(new_connection->sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        fprintf(stderr, "Failed to set sockopt IP_HDRINCL\n");
        exit(EXIT_FAILURE);
    }
    
    const uint16_t clientPort = rand();

    new_connection->packet_queue = (PacketQueue){
        .first_node = NULL,
        .last_node = NULL
    };

    atomic_store(&new_connection->packet_queue.owner, PACKET_QUEUE_OWNER_NONE);

    new_connection->server_addr_len = sizeof(new_connection->server_addr);

    new_connection->port_info.destination_port = port;
    new_connection->port_info.source_port = clientPort;

    atomic_store(&new_connection->packet_handler, NULL);

    memset(&new_connection->server_addr, 0, sizeof(struct sockaddr_in));
    new_connection->server_addr.sin_family = AF_INET;
    new_connection->server_addr.sin_port = htons(port);
    new_connection->server_addr.sin_addr.s_addr = inet_addr(ip_address);

    // Request the server information, and proccess it
    SwiftNetPacketInfo request_server_information_packet_info = {
        .port_info = new_connection->port_info,
        .packet_length = 0x00,
        .packet_type = PACKET_TYPE_REQUEST_INFORMATION,
        .maximum_transmission_unit = maximum_transmission_unit
    };

    const struct ip request_server_info_ip_header = construct_ip_header(new_connection->server_addr.sin_addr, PACKET_HEADER_SIZE, rand());

    uint8_t request_server_info_buffer[PACKET_HEADER_SIZE];

    memcpy(request_server_info_buffer, &request_server_info_ip_header, sizeof(struct ip));
    memcpy(request_server_info_buffer + sizeof(struct ip), &request_server_information_packet_info, sizeof(SwiftNetPacketInfo));

    const uint16_t checksum = crc16(request_server_info_buffer, sizeof(request_server_info_buffer));

    memcpy(request_server_info_buffer + offsetof(struct ip, ip_sum), &checksum, SIZEOF_FIELD(struct ip, ip_sum));

    memset(&new_connection->packet_callback_queue, 0x00, sizeof(PacketCallbackQueue));
    atomic_store(&new_connection->packet_callback_queue.owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

    uint8_t server_information_buffer[PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation)];

    pthread_t send_request_thread;

    const RequestServerInformationArgs thread_args = {
        .sockfd = new_connection->sockfd,
        .data = request_server_info_buffer,
        .size = sizeof(request_server_info_buffer),
        .server_addr = new_connection->server_addr,
        .server_addr_len = sizeof(new_connection->server_addr)
    };

    pthread_create(&send_request_thread, NULL, request_server_information, (void*)&thread_args);

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    while(1) {
        const int bytes_received = recvfrom(new_connection->sockfd, server_information_buffer, sizeof(server_information_buffer), 0x00, NULL, NULL);
        if(bytes_received < 0){
           SwiftNetDebug(
               if(check_debug_flag(DEBUG_INITIALIZATION)){
                   send_debug_message("Server did not respond within 5 second\n");
                }
           )
           exit_thread = true;
           pthread_join(send_request_thread, NULL);
           return NULL;
        }
            
        if(bytes_received != PACKET_HEADER_SIZE) {
            SwiftNetDebug(
                if (check_debug_flag(DEBUG_INITIALIZATION)) {
                    send_debug_message("Invalid packet received from server. Expected server information: {\"bytes_received\": %d, \"expected_bytes\": %d}\n", bytes_received, PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation));
                }
            )

            continue;
        }

        const struct ip* const restrict ip_header = (struct ip*)&server_information_buffer;

        const SwiftNetPacketInfo* const restrict packet_info = (SwiftNetPacketInfo *)&server_information_buffer[sizeof(struct ip)];

        if(packet_info->port_info.destination_port != new_connection->port_info.source_port || packet_info->port_info.source_port != new_connection->port_info.destination_port) {
            SwiftNetDebug(
                if (check_debug_flag(DEBUG_INITIALIZATION)) {
                    send_debug_message("Port info does not match: {\"destination_port\": %d, \"source_port\": %d, \"source_ip_address\": \"%s\"}\n", packet_info->port_info.destination_port, packet_info->port_info.source_port, inet_ntoa(ip_header->ip_src));
                }
            )

            continue;
        }

        if(packet_info->packet_type != PACKET_TYPE_REQUEST_INFORMATION) {
            SwiftNetDebug(
                if (check_debug_flag(DEBUG_INITIALIZATION)) {
                    send_debug_message("Invalid packet type: {\"packet_type\": %d}\n", packet_info->packet_type);
                }
            )
            continue;
        }
            
        if(bytes_received != 0) {
            break;
        }
    }

    exit_thread = true;

    pthread_join(send_request_thread, NULL);

    const SwiftNetPacketInfo* const restrict packet_info = (SwiftNetPacketInfo*)&server_information_buffer[sizeof(struct ip)];

    const SwiftNetServerInformation* const restrict server_information = (SwiftNetServerInformation*)&server_information_buffer[PACKET_HEADER_SIZE];

    new_connection->maximum_transmission_unit = packet_info->maximum_transmission_unit;

    new_connection->pending_messages_memory_allocator = allocator_create(sizeof(SwiftNetPendingMessage), 100);
    new_connection->pending_messages = vector_create(100);
    new_connection->packets_sending_memory_allocator = allocator_create(sizeof(SwiftNetPacketSending), 100);
    new_connection->packets_sending = vector_create(100);
    new_connection->packets_completed_memory_allocator = allocator_create(sizeof(SwiftNetPacketCompleted), 100);
    new_connection->packets_completed = vector_create(100);

    pthread_create(&new_connection->handle_packets_thread, NULL, swiftnet_client_handle_packets, new_connection);

    SwiftNetDebug(
        if (check_debug_flag(DEBUG_INITIALIZATION)) {
            send_debug_message("Successfully initialized client\n");
        }
    )

    return new_connection;
}
