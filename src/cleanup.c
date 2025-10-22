#include "internal/internal.h"
#include "swift_net.h"

void swiftnet_cleanup() {
    allocator_destroy(&packet_queue_node_memory_allocator);
    allocator_destroy(&packet_callback_queue_node_memory_allocator);
    allocator_destroy(&server_packet_data_memory_allocator);
    allocator_destroy(&client_packet_data_memory_allocator);
    allocator_destroy(&packet_buffer_memory_allocator);
    allocator_destroy(&server_memory_allocator);

    #ifdef SWIFT_NET_REQUESTS
        allocator_destroy(&requests_sent_memory_allocator);

        vector_destroy(&requests_sent);
    #endif
}
