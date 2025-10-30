#include "internal/internal.h"
#include "swift_net.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static PacketCallbackQueueNode* const wait_for_next_packet_callback(PacketCallbackQueue* const packet_queue) {
    uint32_t owner_none = PACKET_CALLBACK_QUEUE_OWNER_NONE;
    while(!atomic_compare_exchange_strong(&packet_queue->owner, &owner_none, PACKET_CALLBACK_QUEUE_OWNER_EXECUTE_PACKET_CALLBACK)) {
        owner_none = PACKET_CALLBACK_QUEUE_OWNER_NONE;
    }

    if(packet_queue->first_node == NULL) {
        atomic_store_explicit(&packet_queue->owner, PACKET_CALLBACK_QUEUE_OWNER_NONE, memory_order_release);
        return NULL;
    }

    printf("found node\n");

    PacketCallbackQueueNode* const node_to_process = packet_queue->first_node;

    if(node_to_process->next == NULL) {
        packet_queue->first_node = NULL;
        packet_queue->last_node = NULL;

        atomic_store_explicit(&packet_queue->owner, PACKET_CALLBACK_QUEUE_OWNER_NONE, memory_order_release);

        return node_to_process;
    }

    packet_queue->first_node = node_to_process->next;

    atomic_store_explicit(&packet_queue->owner, PACKET_CALLBACK_QUEUE_OWNER_NONE, memory_order_release);

    return node_to_process;
}

void execute_packet_callback(PacketCallbackQueue* const queue, void (* const _Atomic * const packet_handler) (void* const), const uint8_t connection_type, SwiftNetMemoryAllocator* const pending_message_memory_allocator, _Atomic bool* closing, const void* const connection) {
    while (1) {
        if (atomic_load(closing) == true) {
            break;
        }

        const PacketCallbackQueueNode* const node = wait_for_next_packet_callback(queue);
        if(node == NULL) {
            continue;
        }

        atomic_thread_fence(memory_order_acquire);

        if(node->packet_data == NULL) {
            allocator_free(&packet_callback_queue_node_memory_allocator, (void*)node);
            continue;
        }

        void (*const packet_handler_loaded)(void*) = atomic_load(packet_handler);

        #ifdef SWIFT_NET_REQUESTS
            bool is_valid_response = false;

            printf("request response: %d\n", node->request_response);

            if (node->request_response == true) {
                vector_lock(&requests_sent);

                for (uint32_t i = 0; i < requests_sent.size; i++) {
                    RequestSent* const current_request_sent = vector_get(&requests_sent, i);

                    if (current_request_sent == NULL) {
                        continue;
                    }

                    in_addr_t sender;
                    if (connection_type == 0) {
                        sender = ((const SwiftNetClientConnection* const)connection)->server_addr.sin_addr.s_addr;
                    } else {
                        sender = ((const SwiftNetServerPacketData* const)node->packet_data)->metadata.sender.sender_address.sin_addr.s_addr;
                    }

                    printf("current response id: %d\npacket id: %d\nsender: %d\ncurrent response sender: %d\n", current_request_sent->packet_id, node->packet_id, sender, current_request_sent->address);
                    if (current_request_sent->packet_id == node->packet_id) {
                        const in_addr_t first_byte = (sender >> 0) & 0xFF;
                        const in_addr_t second_byte = (sender >> 8) & 0xFF;

                        printf("%d %d\n", first_byte, second_byte);

                        if ((first_byte != 127) && !(first_byte == 192 && second_byte == 168)) {
                            if (current_request_sent->address != sender) {
                                continue;
                            }
                        }

                        current_request_sent->packet_data = node->packet_data;

                        is_valid_response = true;

                        break;
                    }
                }

                vector_unlock(&requests_sent);

                if (is_valid_response == true) {
                    if (node->pending_message != NULL) {
                        free(node->pending_message->chunks_received);
                    }

                    return;
                }
            } else {
                (*packet_handler_loaded)(node->packet_data);
            }
        #else
            (*packet_handler_loaded)(node->packet_data);
        #endif

        if(node->pending_message != NULL) {
            free(node->pending_message->chunks_received);
            allocator_free(pending_message_memory_allocator, node->pending_message);

            if (connection_type == 0) {
                free(((const SwiftNetClientPacketData* const)(node->packet_data))->data);
            } else {
                free(((const SwiftNetServerPacketData* const)(node->packet_data))->data);
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

void* execute_packet_callback_client(void* const void_client) {
    SwiftNetClientConnection* const client = void_client;

    execute_packet_callback(&client->packet_callback_queue, (void*)&client->packet_handler, 0, &client->pending_messages_memory_allocator, void_client, &client->closing);

    return NULL;
}

void* execute_packet_callback_server(void* const void_server) {
    SwiftNetServer* const server = void_server;

    execute_packet_callback(&server->packet_callback_queue, (void*)&server->packet_handler, 1, &server->pending_messages_memory_allocator, void_server, &server->closing);

    return NULL;
}
