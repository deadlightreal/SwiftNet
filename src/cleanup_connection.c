#include "internal/internal.h"
#include "swift_net.h"
#include <stdatomic.h>
#include <stdint.h>
#include <unistd.h>

void swiftnet_client_cleanup(SwiftNetClientConnection* const client) {
    allocator_destroy(&client->packets_sending_memory_allocator);
    allocator_destroy(&client->pending_messages_memory_allocator);
    allocator_destroy(&client->packets_completed_memory_allocator);

    vector_destroy(&client->packets_sending);
    vector_destroy(&client->pending_messages);
    vector_destroy(&client->packets_completed);

    const char* interface_name = client->loopback ? LOOPBACK_INTERFACE_NAME : default_network_interface;

    vector_lock(&listeners);

    for (uint16_t i = 0; i < listeners.size; i++) {
        Listener* const current_listener = vector_get(&listeners, i);
        if (strcmp(interface_name, current_listener->interface_name) == 0) {
            vector_lock(&current_listener->client_connections);

            for (uint16_t inx = 0; inx < current_listener->client_connections.size; inx++) {
                SwiftNetClientConnection* const current_client_conn = vector_get(&current_listener->client_connections, i);
                if (current_client_conn != client) {
                    continue;
                }

                vector_remove(&current_listener->client_connections, inx);

                break;
            }

            vector_unlock(&current_listener->client_connections);

            break;
        }
    }

    vector_unlock(&listeners);

    atomic_store_explicit(&client->closing, true, memory_order_release);

    pthread_join(client->process_packets_thread, NULL);
    pthread_join(client->execute_callback_thread, NULL);

    pcap_close(client->pcap);

    allocator_free(&client_connection_memory_allocator, client);
}

void swiftnet_server_cleanup(SwiftNetServer* const server) {
    allocator_destroy(&server->packets_sending_memory_allocator);
    allocator_destroy(&server->pending_messages_memory_allocator);
    allocator_destroy(&server->packets_completed_memory_allocator);

    vector_destroy(&server->packets_sending);
    vector_destroy(&server->pending_messages);
    vector_destroy(&server->packets_completed);

    const char* interface_name = server->loopback ? LOOPBACK_INTERFACE_NAME : default_network_interface;

    vector_lock(&listeners);

    for (uint16_t i = 0; i < listeners.size; i++) {
        Listener* const current_listener = vector_get(&listeners, i);
        if (strcmp(interface_name, current_listener->interface_name) == 0) {
            vector_lock(&current_listener->servers);

            for (uint16_t inx = 0; inx < current_listener->servers.size; inx++) {
                SwiftNetServer* const current_server = vector_get(&current_listener->servers, i);
                if (current_server != server) {
                    continue;
                }

                vector_remove(&current_listener->servers, inx);

                break;
            }

            vector_unlock(&current_listener->servers);

            break;
        }
    }

    vector_unlock(&listeners);

    atomic_store_explicit(&server->closing, true, memory_order_release);

    pthread_join(server->process_packets_thread, NULL);
    pthread_join(server->execute_callback_thread, NULL);

    pcap_close(server->pcap);

    allocator_free(&server_memory_allocator, server);
}
