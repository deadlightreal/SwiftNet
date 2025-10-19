#pragma once

#ifdef __cplusplus
    extern "C" {

    #define restrict __restrict__
#endif

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

#define PACKET_TYPE_MESSAGE 0x01
#define PACKET_TYPE_REQUEST_INFORMATION 0x02
#define PACKET_TYPE_SEND_LOST_PACKETS_REQUEST 0x03
#define PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE 0x04
#define PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET 0x05

#define PACKET_INFO_ID_NONE 0xFFFF

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

#ifndef SWIFT_NET_DISABLE_ERROR_CHECKING
    #define SwiftNetErrorCheck(code) code
#else
    #define SwiftNetErrorCheck(code)
#endif

#ifndef SWIFT_NET_DISABLE_DEBUGGING
    #define SwiftNetDebug(code) code
#else
    #define SwiftNetDebug(code) code
#endif

extern uint32_t maximum_transmission_unit;

typedef enum {
    DEBUG_PACKETS_SENDING = 1u << 0,
    DEBUG_PACKETS_RECEIVING = 1u << 1,
    DEBUG_INITIALIZATION = 1u << 2,
    DEBUG_LOST_PACKETS = 1u << 3
} SwiftNetDebugFlags;

typedef struct {
    uint32_t flags;
} SwiftNetDebugger;

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
} SwiftNetPacketClientMetadata;

typedef struct {
    uint32_t data_length;
    SwiftNetPortInfo port_info;
    SwiftNetClientAddrData sender;
} SwiftNetPacketServerMetadata;

typedef struct {
    uint32_t packet_length;
    SwiftNetPortInfo port_info;
    uint8_t packet_type;
    uint32_t chunk_amount;
    uint32_t chunk_index;
    uint32_t maximum_transmission_unit;
} SwiftNetPacketInfo;

typedef struct {
    uint32_t maximum_transmission_unit;
} SwiftNetServerInformation;

typedef struct {
    uint16_t packet_id;
    _Atomic volatile bool updated_lost_chunks;
    volatile uint32_t* lost_chunks;
    volatile uint32_t lost_chunks_size;
    _Atomic volatile bool successfully_received;
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
    atomic_uint owner;
    volatile PacketQueueNode* first_node;
    volatile PacketQueueNode* last_node;
} PacketQueue;

typedef struct PacketCallbackQueueNode PacketCallbackQueueNode;

struct PacketCallbackQueueNode {
    void* packet_data;
    SwiftNetPendingMessage* pending_message;
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
    atomic_uint owner;
    volatile PacketCallbackQueueNode* first_node;
    volatile PacketCallbackQueueNode* last_node;
} PacketCallbackQueue;

typedef struct {
    uint16_t packet_id;
    volatile bool confirmed;
} SwiftNetSentSuccessfullyCompletedPacketSignal;

typedef struct {
    uint32_t size;
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
} SwiftNetVector;

// Connection data
typedef struct {
    int sockfd;
    SwiftNetPortInfo port_info;
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;
    _Atomic(void (*)(SwiftNetClientPacketData* restrict const)) packet_handler;
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    uint32_t maximum_transmission_unit;
    volatile SwiftNetVector pending_messages;
    volatile SwiftNetMemoryAllocator pending_messages_memory_allocator;
    volatile SwiftNetVector packets_sending;
    volatile SwiftNetMemoryAllocator packets_sending_memory_allocator;
    volatile SwiftNetVector packets_completed;
    volatile SwiftNetMemoryAllocator packets_completed_memory_allocator;
    uint8_t* current_read_pointer;
    PacketQueue packet_queue;
    PacketCallbackQueue packet_callback_queue;
} SwiftNetClientConnection;

typedef struct {
    int sockfd;
    uint16_t server_port;
    _Atomic(void (*)(SwiftNetServerPacketData* restrict const)) packet_handler;
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    volatile SwiftNetVector pending_messages;
    volatile SwiftNetMemoryAllocator pending_messages_memory_allocator;
    volatile SwiftNetVector packets_sending;
    volatile SwiftNetMemoryAllocator packets_sending_memory_allocator;
    volatile SwiftNetVector packets_completed;
    volatile SwiftNetMemoryAllocator packets_completed_memory_allocator;
    uint8_t* current_read_pointer;
    PacketQueue packet_queue;
    PacketCallbackQueue packet_callback_queue;
} SwiftNetServer;

extern void swiftnet_server_set_message_handler(SwiftNetServer* server, void (*new_handler)(SwiftNetServerPacketData* restrict const));
extern void swiftnet_client_set_message_handler(SwiftNetClientConnection* client, void (*new_handler)(SwiftNetClientPacketData* restrict const));
extern void swiftnet_client_append_to_packet(const void* const restrict data, const uint32_t data_size, SwiftNetPacketBuffer* restrict const packet);
extern void swiftnet_server_append_to_packet(const void* const restrict data, const uint32_t data_size, SwiftNetPacketBuffer* restrict const packet);
extern void swiftnet_client_cleanup(SwiftNetClientConnection* const restrict client);
extern void swiftnet_server_cleanup(SwiftNetServer* const restrict server);
extern void swiftnet_initialize();
extern void* swiftnet_server_handle_packets(void* restrict const server_void);
extern void* swiftnet_client_handle_packets(void* restrict const client_void);
extern void swiftnet_client_send_packet (SwiftNetClientConnection* restrict const client, SwiftNetPacketBuffer* restrict const packet);
extern void swiftnet_server_send_packet (SwiftNetServer* restrict const server, SwiftNetPacketBuffer* restrict const packet, const SwiftNetClientAddrData target);

extern SwiftNetPacketBuffer swiftnet_server_create_packet_buffer(const uint32_t buffer_size);
extern SwiftNetPacketBuffer swiftnet_client_create_packet_buffer(const uint32_t buffer_size);
extern void swiftnet_server_destroy_packet_buffer(SwiftNetPacketBuffer* restrict const packet);
extern void swiftnet_client_destroy_packet_buffer(SwiftNetPacketBuffer* restrict const packet);
extern SwiftNetServer* swiftnet_create_server(const uint16_t port);
extern SwiftNetClientConnection* swiftnet_create_client(const char* const restrict ip_address, const uint16_t port);
extern void* swiftnet_client_read_packet(SwiftNetClientPacketData* restrict const packet_data, const uint32_t data_size);
extern void* swiftnet_server_read_packet(SwiftNetServerPacketData* restrict const packet_data, const uint32_t data_size);

extern void swiftnet_cleanup();

SwiftNetDebug(
    extern void swiftnet_add_debug_flags(const uint32_t flags);
)

#ifdef __cplusplus
    }
#endif
