#include "internal/internal.h"
#include "swift_net.h"
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

void execute_packet_callback(PacketCallbackQueue* restrict const queue, void (* const volatile * const packet_handler) (uint8_t*, void*)) {
    while (1) {
        const volatile PacketCallbackQueueNode* const node = wait_for_next_packet_callback(queue);
        if(node == NULL) {
            continue;
        }

        (*packet_handler)(node->data, node->metadata);

        if(node->pending_message != NULL) {
            free(node->pending_message->chunks_received);
            memset(node->pending_message, 0x00, sizeof(SwiftNetPendingMessage));
        } else {
            free(node->data - PACKET_HEADER_SIZE);
        }

        free(node->metadata);
        free((void*)node);
    }
}

void* execute_packet_callback_client(void* void_client) {
    SwiftNetClientConnection* client = void_client;

    execute_packet_callback(&client->packet_callback_queue, (void (* const volatile * const) (uint8_t*, void*))&client->packet_handler);

    return NULL;
}

void* execute_packet_callback_server(void* void_server) {
    SwiftNetServer* server = void_server;

    execute_packet_callback(&server->packet_callback_queue, (void (* const volatile * const) (uint8_t*, void*))&server->packet_handler);

    return NULL;
}
