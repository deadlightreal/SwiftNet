#include "internal/internal.h"
#include "swift_net.h"

void swiftnet_cleanup() {
    allocator_destroy(&packet_queue_node_memory_allocator);
}
