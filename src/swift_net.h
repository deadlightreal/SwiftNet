#pragma once

#include <stdint.h>
#ifdef __cplusplus
    extern "C" {

    #define restrict __restrict__
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <pthread.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <pcap/pcap.h>

#ifndef SWIFT_NET_DISABLE_ERROR_CHECKING
    #define SWIFT_NET_ERROR
#endif

#ifndef SWIFT_NET_DISABLE_REQUESTS
    #define SWIFT_NET_REQUESTS
#endif

#ifndef SWIFT_NET_DISABLE_DEBUGGING
    #define SWIFT_NET_DEBUG
#endif

#define PACKET_TYPE_MESSAGE 0x01
#define PACKET_TYPE_REQUEST_INFORMATION 0x02
#define PACKET_TYPE_SEND_LOST_PACKETS_REQUEST 0x03
#define PACKET_TYPE_SEND_LOST_PACKETS_RESPONSE 0x04
#define PACKET_TYPE_SUCCESSFULLY_RECEIVED_PACKET 0x05
#define PACKET_TYPE_REQUEST 0x06
#ifdef SWIFT_NET_REQUESTS
#define PACKET_TYPE_RESPONSE 0x07
#endif

#define PACKET_INFO_ID_NONE 0xFFFF

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

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
    struct in_addr sender_address;
    uint32_t maximum_transmission_unit;
    uint16_t port;
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
    uint32_t packet_length;
    SwiftNetPortInfo port_info;
    uint8_t packet_type;
    uint32_t chunk_amount;
    uint32_t chunk_index;
    uint32_t maximum_transmission_unit;
} SwiftNetPacketInfo;

typedef struct {
    uint8_t* packet_data_start;
    SwiftNetPacketInfo packet_info;
    uint16_t packet_id;
    struct in_addr sender_address;
    uint8_t* chunks_received;
    uint32_t chunks_received_length;
    uint32_t chunks_received_number;
} SwiftNetPendingMessage;

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

typedef struct PacketQueueNode PacketQueueNode;

struct PacketQueueNode {
    PacketQueueNode* next;
    uint8_t* data;
    uint32_t data_read;
    struct in_addr sender_address;
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
    SwiftNetPendingMessage* internal_pending_message; // Do not use!!
} SwiftNetServerPacketData;

typedef struct {
    uint8_t* data;
    uint8_t* current_pointer;
    SwiftNetPacketClientMetadata metadata;
    SwiftNetPendingMessage* internal_pending_message; // Do not use!!
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
    pcap_t* pcap;
    struct ether_header eth_header;
    SwiftNetPortInfo port_info;
    struct in_addr server_addr;
    socklen_t server_addr_len;
    _Atomic(void (*)(SwiftNetClientPacketData* const)) packet_handler;
    _Atomic bool closing;
    _Atomic bool initialized;
    uint16_t addr_type;
    bool loopback;
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
    uint8_t prepend_size;
} SwiftNetClientConnection;

typedef struct {
    pcap_t* pcap;
    struct ether_header eth_header;
    uint16_t server_port;
    _Atomic(void (*)(SwiftNetServerPacketData* const)) packet_handler;
    _Atomic bool closing;
    uint16_t addr_type;
    bool loopback;
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
    uint8_t prepend_size;
} SwiftNetServer;

/**
 * @brief Set a custom message handler for the server.
 * @param server Pointer to the server.
 * @param new_handler Function pointer to the new message handler.
 */
extern void swiftnet_server_set_message_handler(
    SwiftNetServer* const server,
    void (* const new_handler)(SwiftNetServerPacketData* const)
);

/**
 * @brief Set a custom message handler for the client connection.
 * @param client Pointer to the client connection.
 * @param new_handler Function pointer to the new message handler.
 */
extern void swiftnet_client_set_message_handler(
    SwiftNetClientConnection* const client,
    void (* const new_handler)(SwiftNetClientPacketData* const)
);

/**
 * @brief Append data to a client packet buffer.
 * @param data Pointer to the data to append.
 * @param data_size Size of the data in bytes.
 * @param packet Pointer to the client packet buffer.
 */
extern void swiftnet_client_append_to_packet(
    const void* const data,
    const uint32_t data_size,
    SwiftNetPacketBuffer* const packet
);

/**
 * @brief Append data to a server packet buffer.
 * @param data Pointer to the data to append.
 * @param data_size Size of the data in bytes.
 * @param packet Pointer to the server packet buffer.
 */
extern void swiftnet_server_append_to_packet(
    const void* const data,
    const uint32_t data_size,
    SwiftNetPacketBuffer* const packet
);

/**
 * @brief Clean up and free resources for a client connection.
 * @param client Pointer to the client connection.
 */
extern void swiftnet_client_cleanup(SwiftNetClientConnection* const client);

/**
 * @brief Clean up and free resources for a server.
 * @param server Pointer to the server.
 */
extern void swiftnet_server_cleanup(SwiftNetServer* const server);

/**
 * @brief Initialize the SwiftNet library. Must be called before using any other functions.
 */
extern void swiftnet_initialize();

/**
 * @brief Send a packet from the client to its connected server.
 * @param client Pointer to the client connection.
 * @param packet Pointer to the packet buffer to send.
 */
extern void swiftnet_client_send_packet(
    SwiftNetClientConnection* const client,
    SwiftNetPacketBuffer* const packet
);

/**
 * @brief Send a packet from the server to a specific client.
 * @param server Pointer to the server.
 * @param packet Pointer to the packet buffer to send.
 * @param target Target client address data.
 */
extern void swiftnet_server_send_packet(
    SwiftNetServer* const server,
    SwiftNetPacketBuffer* const packet,
    const SwiftNetClientAddrData target
);

/**
 * @brief Create a packet buffer for the server.
 * @param buffer_size Size of the buffer in bytes.
 * @return SwiftNetPacketBuffer Initialized packet buffer.
 */
extern SwiftNetPacketBuffer swiftnet_server_create_packet_buffer(const uint32_t buffer_size);

/**
 * @brief Create a packet buffer for the client.
 * @param buffer_size Size of the buffer in bytes.
 * @return SwiftNetPacketBuffer Initialized packet buffer.
 */
extern SwiftNetPacketBuffer swiftnet_client_create_packet_buffer(const uint32_t buffer_size);

/**
 * @brief Destroy a server packet buffer and free resources.
 * @param packet Pointer to the server packet buffer.
 */
extern void swiftnet_server_destroy_packet_buffer(const SwiftNetPacketBuffer* const packet);

/**
 * @brief Destroy a client packet buffer and free resources.
 * @param packet Pointer to the client packet buffer.
 */
extern void swiftnet_client_destroy_packet_buffer(const SwiftNetPacketBuffer* const packet);

/**
 * @brief Create and initialize a server.
 * @param port Port number to bind the server to.
 * @param loopback If true, server binds only to loopback interface.
 * @return Pointer to initialized SwiftNetServer, or NULL if unexpected error happened.
 */
extern SwiftNetServer* swiftnet_create_server(const uint16_t port, const bool loopback);

/**
 * @brief Create and initialize a client connection.
 * @param ip_address IP address of the server to connect to.
 * @param port Server port to connect to.
 * @param timeout_ms Connection timeout in milliseconds.
 * @return Pointer to initialized SwiftNetClientConnection, or NULL if server failed to respond.
 */
extern SwiftNetClientConnection* swiftnet_create_client(
    const char* const ip_address,
    const uint16_t port,
    const uint32_t timeout_ms
);

/**
 * @brief Read data from a client packet.
 * @param packet_data Pointer to client packet data.
 * @param data_size Number of bytes to read.
 * @return Pointer to the read data, or NULL if read more data than there is.
 */
extern void* swiftnet_client_read_packet(SwiftNetClientPacketData* const packet_data, const uint32_t data_size);

/**
 * @brief Read data from a server packet.
 * @param packet_data Pointer to server packet data.
 * @param data_size Number of bytes to read.
 * @return Pointer to the read data, or NULL if read more data than there is.
 */
extern void* swiftnet_server_read_packet(SwiftNetServerPacketData* const packet_data, const uint32_t data_size);

/**
 * @brief Destroy client packet data and release memory.
 * @param packet_data Pointer to client packet data.
 * @param client_conn Pointer to the client connection.
 */
extern void swiftnet_client_destroy_packet_data(
    SwiftNetClientPacketData* const packet_data,
    SwiftNetClientConnection* const client_conn
);

/**
 * @brief Destroy server packet data and release memory.
 * @param packet_data Pointer to server packet data.
 * @param server Pointer to the server.
 */
extern void swiftnet_server_destroy_packet_data(
    SwiftNetServerPacketData* const packet_data,
    SwiftNetServer* const server
);

/**
 * @brief Clean up the entire SwiftNet library.
 */
extern void swiftnet_cleanup();

#ifdef SWIFT_NET_REQUESTS
/**
 * @brief Make a request from a client and wait for a response.
 * @param client Pointer to the client connection.
 * @param packet Pointer to the packet buffer containing the request.
 * @param timeout_ms Timeout in milliseconds.
 * @return Pointer to client packet data containing the response, or NULL if server failed to respond.
 */
extern SwiftNetClientPacketData* swiftnet_client_make_request(
    SwiftNetClientConnection* const client,
    SwiftNetPacketBuffer* const packet,
    const uint32_t timeout_ms
);

/**
 * @brief Make a request from the server to a specific client and wait for response.
 * @param server Pointer to the server.
 * @param packet Pointer to the packet buffer containing the request.
 * @param addr_data Target client address.
 * @param timeout_ms Timeout in milliseconds.
 * @return Pointer to server packet data containing the response, or NULL if client failed to respond.
 */
extern SwiftNetServerPacketData* swiftnet_server_make_request(
    SwiftNetServer* const server,
    SwiftNetPacketBuffer* const packet,
    const SwiftNetClientAddrData addr_data,
    const uint32_t timeout_ms
);

/**
 * @brief Send a response from a client.
 * @param client Pointer to the client connection.
 * @param packet_data Pointer to the request packet data.
 * @param buffer Packet buffer containing the response.
 */
extern void swiftnet_client_make_response(
    SwiftNetClientConnection* const client,
    SwiftNetClientPacketData* const packet_data,
    SwiftNetPacketBuffer* const buffer
);

/**
 * @brief Send a response from the server.
 * @param server Pointer to the server.
 * @param packet_data Pointer to the request packet data.
 * @param buffer Packet buffer containing the response.
*/
extern void swiftnet_server_make_response(
    SwiftNetServer* const server,
    SwiftNetServerPacketData* const packet_data,
    SwiftNetPacketBuffer* const buffer
);
#endif

#ifdef SWIFT_NET_DEBUG
    /**
    * @brief Adds one or more debug flags to the global debugger state.
    *
    * This function performs a bitwise OR on the existing debugger flags,
    * enabling any debug options specified in @p flags while leaving the
    * previously enabled flags untouched.
    *
    * @param flags Bitmask of debug flags to enable. Multiple flags may be
    * combined using bitwise OR.
    */
    extern void swiftnet_add_debug_flags(const uint32_t flags);
#endif

#ifdef __cplusplus
    }
#endif
