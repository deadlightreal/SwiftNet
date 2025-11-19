#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
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

typedef struct {
    pcap_t* pcap;
    const void* const data;
    const uint32_t size;
    const struct in_addr server_addr;
    const uint32_t timeout_ms;
    SwiftNetClientConnection* const connection;
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
            break;
        }

        if(atomic_load_explicit(&request_server_information_args->connection->initialized, memory_order_acquire) == true) {
            return NULL;
        }

        #ifdef SWIFT_NET_DEBUG
            if (check_debug_flag(DEBUG_INITIALIZATION)) {
                send_debug_message("Requested server information: {\"server_ip_address\": \"%s\"}\n", inet_ntoa(request_server_information_args->server_addr));
            }
        #endif

        swiftnet_pcap_send(request_server_information_args->pcap, request_server_information_args->data, request_server_information_args->size);

        usleep(250000);
    }

    return NULL;
}



SwiftNetClientConnection* swiftnet_create_client(const char* const ip_address, const uint16_t port, const uint32_t timeout_ms) {
    SwiftNetClientConnection* const new_connection = allocator_allocate(&client_connection_memory_allocator);

    struct in_addr addr;
    inet_aton(ip_address, &addr);
    const uint32_t ip = ntohl(addr.s_addr);
    const bool loopback = (ip >> 24) == 127;

    new_connection->loopback = loopback;

    new_connection->pcap = swiftnet_pcap_open(loopback ? LOOPBACK_INTERFACE_NAME : default_network_interface);
    if (new_connection->pcap == NULL) {
        fprintf(stderr, "Failed to open bpf\n");
        exit(EXIT_FAILURE);
    }

    new_connection->addr_type = pcap_datalink(new_connection->pcap);

    const uint16_t clientPort = rand();

    const uint8_t prepend_size = PACKET_PREPEND_SIZE(new_connection->addr_type);

    new_connection->prepend_size = prepend_size;

    new_connection->packet_queue = (PacketQueue){
        .first_node = NULL,
        .last_node = NULL
    };

    atomic_store(&new_connection->packet_queue.owner, PACKET_QUEUE_OWNER_NONE);

    new_connection->server_addr_len = sizeof(new_connection->server_addr);

    new_connection->port_info.destination_port = port;
    new_connection->port_info.source_port = clientPort;

    new_connection->packet_handler = NULL;

    new_connection->server_addr.s_addr = inet_addr(ip_address);

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

    const struct ip request_server_info_ip_header = construct_ip_header(new_connection->server_addr, PACKET_HEADER_SIZE, rand());

    HANDLE_PACKET_CONSTRUCTION(&request_server_info_ip_header, &request_server_information_packet_info, new_connection->addr_type, &eth_header, PACKET_HEADER_SIZE + prepend_size, request_server_info_buffer)

    HANDLE_CHECKSUM(request_server_info_buffer, sizeof(request_server_info_buffer), prepend_size)

    memset(&new_connection->packet_callback_queue, 0x00, sizeof(PacketCallbackQueue));
    atomic_store(&new_connection->packet_callback_queue.owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

    pthread_t send_request_thread;

    const RequestServerInformationArgs thread_args = {
        .pcap = new_connection->pcap,
        .data = request_server_info_buffer,
        .size = sizeof(request_server_info_buffer),
        .server_addr = new_connection->server_addr,
        .timeout_ms = timeout_ms,
        .connection = new_connection
    };

    atomic_store_explicit(&new_connection->closing, false, memory_order_release);

    atomic_store_explicit(&new_connection->initialized, false, memory_order_release);

    Listener* const listener = check_existing_listener(loopback ? LOOPBACK_INTERFACE_NAME : default_network_interface, new_connection, CONNECTION_TYPE_CLIENT, loopback);

    pthread_create(&send_request_thread, NULL, request_server_information, (void*)&thread_args);

    pthread_join(send_request_thread, NULL);

    if (atomic_load_explicit(&new_connection->initialized, memory_order_acquire) == false) {
        atomic_store_explicit(&new_connection->closing, true, memory_order_release);

        vector_lock(&listener->client_connections);

        pcap_close(new_connection->pcap);

        allocator_free(&client_connection_memory_allocator, new_connection);

        for (uint16_t i = 0; i < listener->client_connections.size; i++) {
            SwiftNetClientConnection* const client_connection = vector_get(&listener->client_connections, i);
            if (client_connection == new_connection) {
                vector_remove(&listener->client_connections, i);
            }
        }

        vector_unlock(&listener->client_connections);

        return NULL;
    }

    new_connection->pending_messages_memory_allocator = allocator_create(sizeof(SwiftNetPendingMessage), 100);
    new_connection->pending_messages = vector_create(100);
    new_connection->packets_sending_memory_allocator = allocator_create(sizeof(SwiftNetPacketSending), 100);
    new_connection->packets_sending = vector_create(100);
    new_connection->packets_completed_memory_allocator = allocator_create(sizeof(SwiftNetPacketCompleted), 100);
    new_connection->packets_completed = vector_create(100);

    pthread_create(&new_connection->process_packets_thread, NULL, swiftnet_client_process_packets, new_connection);
    pthread_create(&new_connection->execute_callback_thread, NULL, execute_packet_callback_client, new_connection);

    #ifdef SWIFT_NET_DEBUG
        if (check_debug_flag(DEBUG_INITIALIZATION)) {
            send_debug_message("Successfully initialized client\n");
        }
    #endif

    return new_connection;
}
