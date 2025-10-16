#include "internal/internal.h"
#include "swift_net.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

volatile PacketCallbackQueueNode* wait_for_next_packet_callback(PacketCallbackQueue* restrict const packet_queue) {
    uint32_t owner_none = PACKET_CALLBACK_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong(&packet_queue->owner, &owner_none, PACKET_CALLBACK_QUEUE_OWNER_EXECUTE_PACKET_CALLBACK)) {
        owner_none = PACKET_CALLBACK_QUEUE_OWNER_NONE;
    }

    if(packet_queue->first_node == NULL) {
        atomic_store(&packet_queue->owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);
        return NULL;
    }

    volatile PacketCallbackQueueNode* const node_to_process = packet_queue->first_node;

    if(node_to_process->next == NULL) {
        packet_queue->first_node = NULL;
        packet_queue->last_node = NULL;

        atomic_store(&packet_queue->owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

        return node_to_process;
    }

    packet_queue->first_node = node_to_process->next;

    atomic_store(&packet_queue->owner, PACKET_CALLBACK_QUEUE_OWNER_NONE);

    return node_to_process;
}

void execute_packet_callback(PacketCallbackQueue* restrict const queue, void (* const _Atomic * const packet_handler) (void* const restrict), const uint8_t connection_type, volatile SwiftNetMemoryAllocator* const pending_message_memory_allocator) {
    while (1) {
        const volatile PacketCallbackQueueNode* const node = wait_for_next_packet_callback(queue);
        if(node == NULL) {
            continue;
        }

        if(node->packet_data == NULL) {
            allocator_free(&packet_callback_queue_node_memory_allocator, (void*)node);
            continue;
        }

        void (*const packet_handler_loaded)(void*) = atomic_load(packet_handler);

        (*packet_handler_loaded)(node->packet_data);

        if(node->pending_message != NULL) {
            free(node->pending_message->chunks_received);
            allocator_free(pending_message_memory_allocator, node->pending_message);

            if (connection_type == 0) {
                free(((SwiftNetClientPacketData*)(node->packet_data))->data);
            } else {
                free(((SwiftNetServerPacketData*)(node->packet_data))->data);
            }
        } else {
            if (connection_type == 0) {
                allocator_free(&packet_buffer_memory_allocator, ((SwiftNetClientPacketData*)(node->packet_data))->data - PACKET_HEADER_SIZE);
                allocator_free(&client_packet_data_memory_allocator, node->packet_data);
            } else {
                allocator_free(&packet_buffer_memory_allocator, ((SwiftNetServerPacketData*)(node->packet_data))->data - PACKET_HEADER_SIZE);
                allocator_free(&server_packet_data_memory_allocator, node->packet_data);
            }
        }

        allocator_free(&packet_callback_queue_node_memory_allocator, (void*)node);
    }
}

void* execute_packet_callback_client(void* void_client) {
    SwiftNetClientConnection* client = void_client;

    execute_packet_callback(&client->packet_callback_queue, (void*)&client->packet_handler, 0, &client->pending_messages_memory_allocator);

    return NULL;
}

void* execute_packet_callback_server(void* void_server) {
    SwiftNetServer* server = void_server;

    execute_packet_callback(&server->packet_callback_queue, (void*)&server->packet_handler, 1, &server->pending_messages_memory_allocator);

    return NULL;
}
