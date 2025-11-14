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
#include <net/ethernet.h>
#include <net/bpf.h>

static _Atomic bool exit_thread = false;
static _Atomic bool timeout_reached = false;

typedef struct {
    const int bpf;
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

        for (uint32_t i = 0; i < request_server_information_args->size; i++) {
            printf("%d ", ((uint8_t*)request_server_information_args->data)[i]);
        }
        printf("\n");

        write(request_server_information_args->bpf, request_server_information_args->data, request_server_information_args->size);

        usleep(100000);
    }

    return NULL;
}

SwiftNetClientConnection* swiftnet_create_client(const char* const ip_address, const uint16_t port, const uint32_t timeout_ms) {
    SwiftNetClientConnection* const new_connection = allocator_allocate(&client_connection_memory_allocator);

    new_connection->bpf = get_bpf_device();

    bind_bpf_to_interface(new_connection->bpf);
    setup_bpf_settings(new_connection->bpf);

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
    const SwiftNetPacketInfo request_server_information_packet_info = construct_packet_info(
        0x00,
        PACKET_TYPE_REQUEST_INFORMATION,
        1,
        0,
        new_connection->port_info
    );

    struct ether_header eth_header = {
        .ether_dhost = {0xff,0xff,0xff,0xff,0xff,0xff},
        .ether_type = htons(0x0800)
    };

    memcpy(eth_header.ether_shost, mac_address, sizeof(eth_header.ether_shost));

    new_connection->eth_header = eth_header;

    const struct ip request_server_info_ip_header = construct_ip_header(new_connection->server_addr.sin_addr, PACKET_HEADER_SIZE - sizeof(eth_header), rand());

    uint8_t request_server_info_buffer[PACKET_HEADER_SIZE];

    memcpy(request_server_info_buffer, &eth_header, sizeof(eth_header)); 
    memcpy(request_server_info_buffer + sizeof(eth_header), &request_server_info_ip_header, sizeof(struct ip));
    memcpy(request_server_info_buffer + sizeof(struct ip) + sizeof(eth_header), &request_server_information_packet_info, sizeof(SwiftNetPacketInfo));

    const uint16_t checksum = crc16(request_server_info_buffer, sizeof(request_server_info_buffer));

    memcpy(request_server_info_buffer + offsetof(struct ip, ip_sum), &checksum, SIZEOF_FIELD(struct ip, ip_sum));

    memset(&new_connection->packet_callback_queue, 0x00, sizeof(PacketCallbackQueue));
    atomic_store(&new_connection->packet_callback_queue.owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

    pthread_t send_request_thread;

    const RequestServerInformationArgs thread_args = {
        .bpf = new_connection->bpf,
        .data = request_server_info_buffer,
        .size = sizeof(request_server_info_buffer),
        .server_addr = new_connection->server_addr,
        .server_addr_len = sizeof(new_connection->server_addr),
        .timeout_ms = timeout_ms
    };

    pthread_create(&send_request_thread, NULL, request_server_information, (void*)&thread_args);

    SwiftNetServerInformation* server_information;
    SwiftNetPacketInfo* packet_info;
    struct ip* ip_header;

    uint8_t buffer[10000];
    
    while(1) {
        const int data_read = read(new_connection->bpf, buffer, sizeof(buffer));
        if (data_read <= 0) {
            usleep(1000);
            continue;
        }

        uint32_t offset = 0;

        while (offset < data_read) {
            struct bpf_hdr *hdr = (struct bpf_hdr *)(buffer + offset);
            unsigned char *data = buffer + offset + hdr->bh_hdrlen;
            uint32_t bytes_received = hdr->bh_caplen;

            if(bytes_received != PACKET_HEADER_SIZE) {
                if (atomic_load_explicit(&timeout_reached, memory_order_acquire) == true) {
                    pthread_join(send_request_thread, NULL);

                    return NULL;
                }

                #ifdef SWIFT_NET_DEBUG
                    if (check_debug_flag(DEBUG_INITIALIZATION)) {
                        send_debug_message("Invalid packet received from server. Expected server information: {\"bytes_received\": %u, \"expected_bytes\": %u}\n", bytes_received, PACKET_HEADER_SIZE + sizeof(SwiftNetServerInformation));
                    }
                #endif

                continue;
            }

            ip_header = (struct ip*)(data + sizeof(struct ether_header));
            packet_info = (SwiftNetPacketInfo*)(data + sizeof(struct ether_header) + sizeof(struct ip));
            server_information = (SwiftNetServerInformation*)(data + sizeof(struct ether_header) + sizeof(struct ip) + sizeof(SwiftNetPacketInfo));
    
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
    
            offset += BPF_WORDALIGN(hdr->bh_hdrlen + bytes_received);
        }
    }

    atomic_store(&exit_thread, true);

    pthread_join(send_request_thread, NULL);

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
