#include "internal.h"
           
void* check_existing_listener(const char* interface_name, void* const connection, const ConnectionType connection_type, const bool loopback) {
    vector_lock(&listeners);

    for (uint16_t i = 0; i < listeners.size; i++) {
        Listener* const current_listener = vector_get(&listeners, i);
        if (strcmp(interface_name, current_listener->interface_name) == 0) {
            if (connection_type == CONNECTION_TYPE_CLIENT) {
                vector_lock(&current_listener->client_connections);

                vector_push(&current_listener->client_connections, connection);

                vector_unlock(&current_listener->client_connections);
            } else {
                vector_lock(&current_listener->servers);

                vector_push(&current_listener->servers, connection);

                vector_unlock(&current_listener->servers);
            }

            vector_unlock(&listeners);

            return current_listener;
        }
    }

    Listener* const new_listener = allocator_allocate(&listener_memory_allocator);
    new_listener->servers = vector_create(10);
    new_listener->client_connections = vector_create(10);
    new_listener->pcap = swiftnet_pcap_open(interface_name);
    memcpy(new_listener->interface_name, interface_name, strlen(interface_name) + 1);
    new_listener->loopback = loopback;

    if (connection_type == CONNECTION_TYPE_CLIENT) {
        vector_push(&new_listener->client_connections, connection);
    } else {
        vector_push(&new_listener->servers, connection);
    }

    vector_push(&listeners, new_listener);

    vector_unlock(&listeners);

    pthread_create(&new_listener->listener_thread, NULL, interface_start_listening, new_listener);

    return new_listener;
}
