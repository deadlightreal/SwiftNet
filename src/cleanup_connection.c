#include "internal/internal.h"
#include "swift_net.h"
#include <stdatomic.h>
#include <unistd.h>

void swiftnet_client_cleanup(SwiftNetClientConnection* const client) {
    allocator_destroy(&client->packets_sending_memory_allocator);
    allocator_destroy(&client->pending_messages_memory_allocator);
    allocator_destroy(&client->packets_completed_memory_allocator);

    vector_destroy(&client->packets_sending);
    vector_destroy(&client->pending_messages);
    vector_destroy(&client->packets_completed);

    atomic_store_explicit(&client->closing, true, memory_order_release);

    shutdown(client->sockfd, SHUT_RD);

    close(client->sockfd);

    pthread_detach(client->handle_packets_thread);
    pthread_detach(client->process_packets_thread);
    pthread_detach(client->execute_callback_thread);

    allocator_free(&client_connection_memory_allocator, client);
}

void swiftnet_server_cleanup(SwiftNetServer* const server) {
    allocator_destroy(&server->packets_sending_memory_allocator);
    allocator_destroy(&server->pending_messages_memory_allocator);
    allocator_destroy(&server->packets_completed_memory_allocator);

    vector_destroy(&server->packets_sending);
    vector_destroy(&server->pending_messages);
    vector_destroy(&server->packets_completed);

    atomic_store_explicit(&server->closing, true, memory_order_release);

    close(server->sockfd);

    pthread_detach(server->handle_packets_thread);
    pthread_detach(server->process_packets_thread);
    pthread_detach(server->execute_callback_thread);

    allocator_free(&server_memory_allocator, server);
}
