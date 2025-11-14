#include <stdatomic.h>
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

SwiftNetServer* swiftnet_create_server(const uint16_t port, const bool loopback) {
    SwiftNetServer* const new_server = allocator_allocate(&server_memory_allocator);

    #ifdef SWIFT_NET_ERROR
        if(unlikely(new_server == NULL)) {
            fprintf(stderr, "Failed to get an empty server\n");
            exit(EXIT_FAILURE);
        }
    #endif

    new_server->server_port = port;
    new_server->loopback = loopback;

    // Init pcap device
    new_server->pcap = swiftnet_pcap_open(loopback ? LOOPBACK_INTERFACE_NAME : default_network_interface);
    if (new_server->pcap == NULL) {
        fprintf(stderr, "Failed to open bpf\n");
        exit(EXIT_FAILURE);
    }

    new_server->addr_type = pcap_datalink(new_server->pcap);

    new_server->prepend_size = PACKET_PREPEND_SIZE(new_server->addr_type);

    struct ether_header eth_header = {
        .ether_dhost = {0xff,0xff,0xff,0xff,0xff,0xff},
        .ether_type = htons(0x0800)
    };

    memcpy(eth_header.ether_shost, mac_address, sizeof(eth_header.ether_shost));

    new_server->eth_header = eth_header;

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

    atomic_store_explicit(&new_server->closing, false, memory_order_release);

    // Create a new thread that will handle all packets received
    check_existing_listener(loopback ? LOOPBACK_INTERFACE_NAME : default_network_interface, new_server, CONNECTION_TYPE_SERVER, loopback);
    pthread_create(&new_server->process_packets_thread, NULL, swiftnet_server_process_packets, new_server);
    pthread_create(&new_server->execute_callback_thread, NULL, execute_packet_callback_server, new_server);

    #ifdef SWIFT_NET_DEBUG
        if (check_debug_flag(DEBUG_INITIALIZATION)) {
            send_debug_message("Successfully initialized server\n");
        }
    #endif

    return new_server;
}
