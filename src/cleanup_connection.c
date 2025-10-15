#include "internal/internal.h"
#include "swift_net.h"
#include <stdlib.h>

void swiftnet_client_cleanup(SwiftNetClientConnection* const restrict client) {
    allocator_free(&client_connection_memory_allocator, client);
}

void swiftnet_server_cleanup(SwiftNetServer* const restrict server) {
    allocator_free(&server_memory_allocator, server);
}
