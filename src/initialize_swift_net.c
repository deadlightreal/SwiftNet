#include <netinet/in.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <stdio.h>
#include "swift_net.h"
#include "internal/internal.h"
#include <unistd.h>

#ifdef SWIFT_NET_DEBUG
    SwiftNetDebugger debugger = {.flags = 0};
#endif

uint32_t maximum_transmission_unit = 0x00;
struct in_addr private_ip_address;

SwiftNetMemoryAllocator packet_queue_node_memory_allocator;
SwiftNetMemoryAllocator packet_callback_queue_node_memory_allocator;
SwiftNetMemoryAllocator server_packet_data_memory_allocator;
SwiftNetMemoryAllocator client_packet_data_memory_allocator;
SwiftNetMemoryAllocator packet_buffer_memory_allocator;
SwiftNetMemoryAllocator server_memory_allocator;
SwiftNetMemoryAllocator client_connection_memory_allocator;
SwiftNetMemoryAllocator pending_message_memory_allocator;

void swiftnet_initialize() {
    int temp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (temp_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

    if (connect(temp_socket, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        fprintf(stderr, "Failed to connect temp socket\n");
        close(temp_socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr private_sockaddr;
    socklen_t private_sockaddr_len = sizeof(private_sockaddr);

    if(getsockname(temp_socket, &private_sockaddr, &private_sockaddr_len) == -1) {
        fprintf(stderr, "Failed to get private ip address\n");
        close(temp_socket);
        exit(EXIT_FAILURE);
    }

    private_ip_address = ((struct sockaddr_in *)&private_sockaddr)->sin_addr;

    char default_network_interface[128];

    const int got_default_interface = get_default_interface(default_network_interface, sizeof(default_network_interface), temp_socket);
    if(unlikely(got_default_interface != 0)) {
        close(temp_socket);
        fprintf(stderr, "Failed to get the default interface\n");
        exit(EXIT_FAILURE);
    }

    maximum_transmission_unit = get_mtu(default_network_interface, temp_socket);
    if(unlikely(maximum_transmission_unit == 0)) {
        close(temp_socket);
        fprintf(stderr, "Failed to get the maximum transmission unit\n");
        exit(EXIT_FAILURE);
    }

    close(temp_socket);

    packet_queue_node_memory_allocator = allocator_create(sizeof(PacketQueueNode), 100);
    packet_callback_queue_node_memory_allocator = allocator_create(sizeof(PacketCallbackQueueNode), 100);
    server_packet_data_memory_allocator = allocator_create(sizeof(SwiftNetServerPacketData), 100);
    client_packet_data_memory_allocator = allocator_create(sizeof(SwiftNetClientPacketData), 100);
    packet_buffer_memory_allocator = allocator_create(maximum_transmission_unit, 100);
    server_memory_allocator = allocator_create(sizeof(SwiftNetServer), 10);
    client_connection_memory_allocator = allocator_create(sizeof(SwiftNetClientConnection), 10);
    pending_message_memory_allocator = allocator_create(sizeof(SwiftNetPendingMessage), 100);

    #ifdef SWIFT_NET_REQUESTS
        requests_sent_memory_allocator = allocator_create(sizeof(RequestSent), 100);

        requests_sent = vector_create(100);
    #endif

    return;
}
