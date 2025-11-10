#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "internal/internal.h"
#include "swift_net.h"
#include <fcntl.h>
#include <stddef.h>
#include <sys/time.h>

static _Atomic bool exit_thread = false;
static _Atomic bool timeout_reached = false;

typedef struct {
    const int sockfd;
    const void* const data;
    const uint32_t size;
    const struct sockaddr_in server_addr;
    const socklen_t server_addr_len;
    const uint32_t timeout_ms;
} RequestServerInformationArgs;

void* request_server_information(void* const request_server_information_args_void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint32_t start = (uint32_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    const RequestServerInformationArgs* const request_server_information_args = (RequestServerInformationArgs*)request_server_information_args_void;

    while (1) {
        struct timeval tv;
        gettimeofday(&tv, NULL);

        uint32_t end = (uint32_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

        if (end > start + request_server_information_args->timeout_ms) {
            atomic_store_explicit(&timeout_reached, true, memory_order_release);

            break;
        }

        if(atomic_load(&exit_thread) == true) {
            atomic_store(&exit_thread, false);

            return NULL;
        }

        #ifdef SWIFT_NET_DEBUG
            if (check_debug_flag(DEBUG_INITIALIZATION)) {
                send_debug_message("Requested server information: {\"server_ip_address\": \"%s\"}\n", inet_ntoa(request_server_information_args->server_addr.sin_addr));
            }
        #endif

        sendto(request_server_information_args->sockfd, request_server_information_args->data, request_server_information_args->size, 0, (struct sockaddr *)&request_server_information_args->server_addr, request_server_information_args->server_addr_len);

        usleep(100000);
    }

    return NULL;
}

SwiftNetClientConnection* swiftnet_create_client(const char* const ip_address, const uint16_t port, const uint32_t timeout_ms) {
    SwiftNetClientConnection* const new_connection = allocator_allocate(&client_connection_memory_allocator);

    new_connection->sockfd = socket(AF_INET, SOCK_RAW, PROTOCOL_NUMBER);
    if(unlikely(new_connection->sockfd < 0)) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    const int on = 1;
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

    new_connection->packet_handler = NULL;

    new_connection->server_addr = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {.s_addr = inet_addr(ip_address)},
        .sin_zero = 0
	#ifdef MACOS
	    , .sin_len = 0
	#endif
    };

    // Request the server information, and proccess it
    const SwiftNetPacketInfo request_server_information_packet_info = {
        .port_info = new_connection->port_info,
        .packet_length = 0x00,
        .packet_type = PACKET_TYPE_REQUEST_INFORMATION,
        .maximum_transmission_unit = maximum_transmission_unit,
        .request_response = false
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
        .server_addr_len = sizeof(new_connection->server_addr),
        .timeout_ms = timeout_ms
    };

    pthread_create(&send_request_thread, NULL, request_server_information, (void*)&thread_args);
    
    while(1) {
        const int bytes_received = recvfrom(new_connection->sockfd, server_information_buffer, sizeof(server_information_buffer), 0x00, NULL, NULL);
        if(bytes_received != PACKET_HEADER_SIZE) {
            if (atomic_load_explicit(&timeout_reached, memory_order_acquire) == true) {
                pthread_join(send_request_thread, NULL);

                return NULL;
            }

            #ifdef SWIFT_NET_DEBUG
                if (check_debug_flag(DEBUG_INITIALIZATION)) {
                    send_debug_message("Invalid packet received from server. Expected server information: {\"bytes_received\": %d, \"expected_bytes\": %d}\n", bytes_received, PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation));
                }
            #endif

            continue;
        }

        const struct ip* const ip_header = (struct ip*)&server_information_buffer;

        const SwiftNetPacketInfo* const packet_info = (SwiftNetPacketInfo *)&server_information_buffer[sizeof(struct ip)];

        if(packet_info->port_info.destination_port != new_connection->port_info.source_port || packet_info->port_info.source_port != new_connection->port_info.destination_port) {
            #ifdef SWIFT_NET_DEBUG
                if (check_debug_flag(DEBUG_INITIALIZATION)) {
                    send_debug_message("Port info does not match: {\"destination_port\": %d, \"source_port\": %d, \"source_ip_address\": \"%s\"}\n", packet_info->port_info.destination_port, packet_info->port_info.source_port, inet_ntoa(ip_header->ip_src));
                }
            #endif

            continue;
        }

        if(packet_info->packet_type != PACKET_TYPE_REQUEST_INFORMATION) {
            #ifdef SWIFT_NET_DEBUG
                if (check_debug_flag(DEBUG_INITIALIZATION)) {
                    send_debug_message("Invalid packet type: {\"packet_type\": %d}\n", packet_info->packet_type);
                }
            #endif
            continue;
        }
            
        if(bytes_received != 0) {
            break;
        }
    }

    atomic_store(&exit_thread, true);

    pthread_join(send_request_thread, NULL);

    const SwiftNetPacketInfo* const packet_info = (SwiftNetPacketInfo*)&server_information_buffer[sizeof(struct ip)];

    const SwiftNetServerInformation* const server_information = (SwiftNetServerInformation*)&server_information_buffer[PACKET_HEADER_SIZE];

    new_connection->maximum_transmission_unit = packet_info->maximum_transmission_unit;

    new_connection->pending_messages_memory_allocator = allocator_create(sizeof(SwiftNetPendingMessage), 100);
    new_connection->pending_messages = vector_create(100);
    new_connection->packets_sending_memory_allocator = allocator_create(sizeof(SwiftNetPacketSending), 100);
    new_connection->packets_sending = vector_create(100);
    new_connection->packets_completed_memory_allocator = allocator_create(sizeof(SwiftNetPacketCompleted), 100);
    new_connection->packets_completed = vector_create(100);

    atomic_store_explicit(&new_connection->closing, false, memory_order_release);

    pthread_create(&new_connection->handle_packets_thread, NULL, swiftnet_client_handle_packets, new_connection);
    pthread_create(&new_connection->process_packets_thread, NULL, swiftnet_client_process_packets, new_connection);
    pthread_create(&new_connection->execute_callback_thread, NULL, execute_packet_callback_client, new_connection);

    #ifdef SWIFT_NET_DEBUG
        if (check_debug_flag(DEBUG_INITIALIZATION)) {
            send_debug_message("Successfully initialized client\n");
        }
    #endif

    return new_connection;
}
