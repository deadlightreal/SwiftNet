#include "internal/internal.h"
#include "swift_net.h"
#include <stdlib.h>

void swiftnet_client_cleanup(SwiftNetClientConnection* const restrict client) {
    allocator_destroy(&client->packets_sending_memory_allocator);
    allocator_destroy(&client->pending_messages_memory_allocator);
    allocator_destroy(&client->packets_completed_memory_allocator);

    vector_destroy(&client->packets_sending);
    vector_destroy(&client->pending_messages);
    vector_destroy(&client->packets_completed);

    allocator_free(&client_connection_memory_allocator, client);
}

void swiftnet_server_cleanup(SwiftNetServer* const restrict server) {
    allocator_destroy(&server->packets_sending_memory_allocator);
    allocator_destroy(&server->pending_messages_memory_allocator);
    allocator_destroy(&server->packets_completed_memory_allocator);

    vector_destroy(&server->packets_sending);
    vector_destroy(&server->pending_messages);
    vector_destroy(&server->packets_completed);

    allocator_free(&server_memory_allocator, server);
}
