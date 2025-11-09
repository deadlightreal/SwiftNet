#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
    extern "C" {

    #define restrict __restrict__
#endif

#define PACKET_TYPE_MESSAGE 0x01
#define PACKET_TYPE_REQUEST_INFORMATION 0x02
#define PACKET_TYPE_SEND_LOST_PACKETS_REQUEST 0x03
#define PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE 0x04
#define PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET 0x05
#define PACKET_TYPE_REQUEST 0x06

#define PACKET_INFO_ID_NONE 0xFFFF

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

#ifndef SWIFT_NET_DISABLE_ERROR_CHECKING
    #define SWIFT_NET_ERROR
#endif

#ifndef SWIFT_NET_DISABLE_REQUESTS
    #define SWIFT_NET_REQUESTS
#endif

#ifndef SWIFT_NET_DISABLE_DEBUGGING
    #define SWIFT_NET_DEBUG
#endif

extern uint32_t maximum_transmission_unit;

#ifdef SWIFT_NET_DEBUG
typedef enum {
    DEBUG_PACKETS_SENDING = 1u << 0,
    DEBUG_PACKETS_RECEIVING = 1u << 1,
    DEBUG_INITIALIZATION = 1u << 2,
    DEBUG_LOST_PACKETS = 1u << 3
} SwiftNetDebugFlags;

typedef struct {
    uint32_t flags;
} SwiftNetDebugger;
#endif

typedef struct {
    uint16_t destination_port;
    uint16_t source_port;
} SwiftNetPortInfo;

typedef struct {
    struct sockaddr_in sender_address;
    socklen_t sender_address_length;
    uint32_t maximum_transmission_unit;
} SwiftNetClientAddrData;

typedef struct {
    uint32_t data_length;
    SwiftNetPortInfo port_info;
    uint16_t packet_id;
    #ifdef SWIFT_NET_REQUESTS
        bool expecting_response;
    #endif
} SwiftNetPacketClientMetadata;

typedef struct {
    uint32_t data_length;
    SwiftNetPortInfo port_info;
    SwiftNetClientAddrData sender;
    uint16_t packet_id;
    #ifdef SWIFT_NET_REQUESTS
        bool expecting_response;
    #endif
} SwiftNetPacketServerMetadata;

typedef struct {
    uint32_t packet_length;
    SwiftNetPortInfo port_info;
    uint8_t packet_type;
    uint32_t chunk_amount;
    uint32_t chunk_index;
    uint32_t maximum_transmission_unit;
    #ifdef SWIFT_NET_REQUESTS
        bool request_response;
    #endif
} SwiftNetPacketInfo;

typedef struct {
    uint32_t maximum_transmission_unit;
} SwiftNetServerInformation;

typedef enum {
    NO_UPDATE,
    UPDATED_LOST_CHUNKS,
    SUCCESSFULLY_RECEIVED
} PacketSendingUpdated;

typedef struct {
    uint16_t packet_id;
    uint32_t* lost_chunks;
    uint32_t lost_chunks_size;
    _Atomic PacketSendingUpdated updated;
    _Atomic bool locked;
} SwiftNetPacketSending;

typedef struct {
    uint16_t packet_id;
    uint32_t packet_length;
} SwiftNetPacketCompleted;

typedef struct {
    uint8_t* packet_buffer_start;   // Start of the allocated buffer
    uint8_t* packet_data_start;     // Start of the stored data
    uint8_t* packet_append_pointer; // Current position to append new data
} SwiftNetPacketBuffer;

typedef struct {
    uint8_t* packet_data_start;
    SwiftNetPacketInfo packet_info;
    uint16_t packet_id;
    in_addr_t sender_address;
    uint8_t* chunks_received;
    uint32_t chunks_received_length;
    uint32_t chunks_received_number;
} SwiftNetPendingMessage;

typedef struct PacketQueueNode PacketQueueNode;

struct PacketQueueNode {
    PacketQueueNode* next;
    uint8_t* data;
    uint32_t data_read;
    struct sockaddr_in sender_address;
    socklen_t server_address_length;
};

typedef struct {
    _Atomic uint32_t owner;
    PacketQueueNode* first_node;
    PacketQueueNode* last_node;
} PacketQueue;

typedef struct PacketCallbackQueueNode PacketCallbackQueueNode;

struct PacketCallbackQueueNode {
    void* packet_data;
    SwiftNetPendingMessage* pending_message;
    uint16_t packet_id;
    PacketCallbackQueueNode* next;
};

typedef struct {
    uint8_t* data;
    uint8_t* current_pointer;
    SwiftNetPacketServerMetadata metadata;
} SwiftNetServerPacketData;

typedef struct {
    uint8_t* data;
    uint8_t* current_pointer;
    SwiftNetPacketClientMetadata metadata;
} SwiftNetClientPacketData;

typedef struct {
    _Atomic uint32_t owner;
    PacketCallbackQueueNode* first_node;
    PacketCallbackQueueNode* last_node;
} PacketCallbackQueue;

typedef struct {
    uint16_t packet_id;
    bool confirmed;
} SwiftNetSentSuccessfullyCompletedPacketSignal;

typedef struct {
    _Atomic uint32_t size;
    void* data;
    _Atomic(void*) next;
    _Atomic(void*) previous;
    _Atomic uint8_t owner;
} SwiftNetMemoryAllocatorStack;

typedef struct {
    _Atomic(void*) first_item;
    _Atomic(void*) last_item;
} SwiftNetChunkStorageManager;

typedef struct {
    SwiftNetChunkStorageManager free_memory_pointers;
    SwiftNetChunkStorageManager data;
    uint32_t item_size;
    uint32_t chunk_item_amount;
    _Atomic uint8_t creating_stack;
} SwiftNetMemoryAllocator;

typedef struct {
    void* data;
    uint32_t size;
    uint32_t capacity;
    _Atomic uint8_t locked;
} SwiftNetVector;

// Connection data
typedef struct {
    int sockfd;
    SwiftNetPortInfo port_info;
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;
    _Atomic(void (*)(SwiftNetClientPacketData* const)) packet_handler;
    _Atomic bool closing;
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    pthread_t execute_callback_thread;
    uint32_t maximum_transmission_unit;
    SwiftNetVector pending_messages;
    SwiftNetMemoryAllocator pending_messages_memory_allocator;
    SwiftNetVector packets_sending;
    SwiftNetMemoryAllocator packets_sending_memory_allocator;
    SwiftNetVector packets_completed;
    SwiftNetMemoryAllocator packets_completed_memory_allocator;
    PacketQueue packet_queue;
    PacketCallbackQueue packet_callback_queue;
} SwiftNetClientConnection;

typedef struct {
    int sockfd;
    uint16_t server_port;
    _Atomic(void (*)(SwiftNetServerPacketData* const)) packet_handler;
    _Atomic bool closing;
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    pthread_t execute_callback_thread;
    SwiftNetVector pending_messages;
    SwiftNetMemoryAllocator pending_messages_memory_allocator;
    SwiftNetVector packets_sending;
    SwiftNetMemoryAllocator packets_sending_memory_allocator;
    SwiftNetVector packets_completed;
    SwiftNetMemoryAllocator packets_completed_memory_allocator;
    uint8_t* current_read_pointer;
    PacketQueue packet_queue;
    PacketCallbackQueue packet_callback_queue;
} SwiftNetServer;

extern void swiftnet_server_set_message_handler(volatile SwiftNetServer* const server, void (* const new_handler)(SwiftNetServerPacketData* const));
extern void swiftnet_client_set_message_handler(volatile SwiftNetClientConnection* const client, void (* const new_handler)(SwiftNetClientPacketData* const));
extern void swiftnet_client_append_to_packet(const void* const data, const uint32_t data_size, SwiftNetPacketBuffer* const packet);
extern void swiftnet_server_append_to_packet(const void* const data, const uint32_t data_size, SwiftNetPacketBuffer* const packet);
extern void swiftnet_client_cleanup(SwiftNetClientConnection* const client);
extern void swiftnet_server_cleanup(SwiftNetServer* const server);
extern void swiftnet_initialize();
extern void* swiftnet_server_handle_packets(void* const server_void);
extern void* swiftnet_client_handle_packets(void* const client_void);
extern void swiftnet_client_send_packet (SwiftNetClientConnection* const client, SwiftNetPacketBuffer* const packet);
extern void swiftnet_server_send_packet (SwiftNetServer* const server, SwiftNetPacketBuffer* const packet, const SwiftNetClientAddrData target);

extern SwiftNetPacketBuffer swiftnet_server_create_packet_buffer(const uint32_t buffer_size);
extern SwiftNetPacketBuffer swiftnet_client_create_packet_buffer(const uint32_t buffer_size);
extern void swiftnet_server_destroy_packet_buffer(const SwiftNetPacketBuffer* const packet);
extern void swiftnet_client_destroy_packet_buffer(const SwiftNetPacketBuffer* const packet);
extern SwiftNetServer* swiftnet_create_server(const uint16_t port);
extern SwiftNetClientConnection* swiftnet_create_client(const char* const ip_address, const uint16_t port, const uint32_t timeout_ms);
extern void* swiftnet_client_read_packet(SwiftNetClientPacketData* const packet_data, const uint32_t data_size);
extern void* swiftnet_server_read_packet(SwiftNetServerPacketData* const packet_data, const uint32_t data_size);
extern void swiftnet_client_destory_packet_data(SwiftNetClientPacketData* const packet_data);
extern void swiftnet_server_destory_packet_data(SwiftNetServerPacketData* const packet_data);

extern void swiftnet_cleanup();

#ifdef SWIFT_NET_REQUESTS
    extern SwiftNetClientPacketData* swiftnet_client_make_request(SwiftNetClientConnection* const client, SwiftNetPacketBuffer* const packet, const uint32_t timeout_ms);
    extern SwiftNetServerPacketData* swiftnet_server_make_request(SwiftNetServer* const server, SwiftNetPacketBuffer* const packet, const SwiftNetClientAddrData addr_data, const uint32_t timeout_ms);

    extern void swiftnet_client_make_response(SwiftNetClientConnection* const client, SwiftNetClientPacketData* const packet_data, SwiftNetPacketBuffer* const buffer);
    extern void swiftnet_server_make_response(SwiftNetServer* const server, SwiftNetServerPacketData* const packet_data, SwiftNetPacketBuffer* const buffer);
#endif

#ifdef SWIFT_NET_DEBUG
    extern void swiftnet_add_debug_flags(const uint32_t flags);
#endif

#ifdef __cplusplus
    }
#endif
