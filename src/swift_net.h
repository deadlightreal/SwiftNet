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

#define MAX_CLIENT_CONNECTIONS 0x0A
#define MAX_SERVERS 0x0A
#define MAX_PENDING_MESSAGES 0x0A
#define MAX_PACKETS_SENDING 0x0A
#define MAX_SENT_SUCCESSFULLY_COMPLETED_PACKET_SIGNAL 0x64
#define MAX_COMPLETED_PACKETS_HISTORY_SIZE 0x64

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

extern uint32_t maximum_transmission_unit;

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
    uint16_t packet_id;
    uint8_t packet_type;
    uint32_t checksum;
    uint32_t chunk_amount;
    uint32_t chunk_index;
    uint32_t chunk_size;
    uint32_t maximum_transmission_unit;
} SwiftNetPacketInfo;

typedef struct {
    uint32_t maximum_transmission_unit;
} SwiftNetServerInformation;

typedef struct {
    uint16_t packet_id;
    volatile bool updated_lost_chunks;
    volatile uint32_t* lost_chunks;
    volatile uint32_t lost_chunks_size;
    volatile bool successfully_received;
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
    uint8_t* data;
    void* metadata;
    SwiftNetPendingMessage* pending_message;
    PacketCallbackQueueNode* next;
};

typedef struct {
    atomic_uint owner;
    volatile PacketCallbackQueueNode* first_node;
    volatile PacketCallbackQueueNode* last_node;
} PacketCallbackQueue;

typedef struct {
    uint16_t packet_id;
    volatile bool confirmed;
} SwiftNetSentSuccessfullyCompletedPacketSignal;

// Connection data
typedef struct {
    int sockfd;
    SwiftNetPortInfo port_info;
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;
    void (* volatile packet_handler)(uint8_t*, SwiftNetPacketClientMetadata* restrict const);
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    uint32_t maximum_transmission_unit;
    SwiftNetPendingMessage pending_messages[MAX_PENDING_MESSAGES];
    volatile SwiftNetPacketSending packets_sending[MAX_PACKETS_SENDING];
    volatile SwiftNetPacketCompleted packets_completed_history[MAX_COMPLETED_PACKETS_HISTORY_SIZE];
    SwiftNetSentSuccessfullyCompletedPacketSignal sent_successfully_completed_packet_signal[MAX_SENT_SUCCESSFULLY_COMPLETED_PACKET_SIGNAL];
    uint8_t* current_read_pointer;
    PacketQueue packet_queue;
    PacketCallbackQueue packet_callback_queue;
} SwiftNetClientConnection;

extern SwiftNetClientConnection SwiftNetClientConnections[MAX_CLIENT_CONNECTIONS];

typedef struct {
    int sockfd;
    uint16_t server_port;
    void (* volatile packet_handler)(uint8_t*, SwiftNetPacketServerMetadata* restrict const);
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    SwiftNetPendingMessage pending_messages[MAX_PENDING_MESSAGES];
    volatile SwiftNetPacketSending packets_sending[MAX_PACKETS_SENDING];
    volatile SwiftNetPacketCompleted packets_completed_history[MAX_COMPLETED_PACKETS_HISTORY_SIZE];
    SwiftNetSentSuccessfullyCompletedPacketSignal sent_successfully_completed_packet_signal[MAX_SENT_SUCCESSFULLY_COMPLETED_PACKET_SIGNAL];
    uint8_t* current_read_pointer;
    PacketQueue packet_queue;
    PacketCallbackQueue packet_callback_queue;
} SwiftNetServer;

extern SwiftNetServer SwiftNetServers[MAX_SERVERS];

extern void swiftnet_server_set_message_handler(SwiftNetServer* server, void (*new_handler)(uint8_t*, SwiftNetPacketServerMetadata* restrict const));
extern void swiftnet_client_set_message_handler(SwiftNetClientConnection* client, void (*new_handler)(uint8_t*, SwiftNetPacketClientMetadata* restrict const));
extern void swiftnet_client_append_to_packet(SwiftNetClientConnection* const restrict client, const void* const restrict data, const uint32_t data_size, SwiftNetPacketBuffer* restrict const packet);
extern void swiftnet_server_append_to_packet(SwiftNetServer* const restrict server, const void* const restrict data, const uint32_t data_size, SwiftNetPacketBuffer* restrict const packet);
extern void swiftnet_client_cleanup(const SwiftNetClientConnection* const restrict client);
extern void swiftnet_server_cleanup(const SwiftNetServer* const restrict server);
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

extern void swiftnet_add_debug_flags(const uint32_t flags);

#ifdef __cplusplus
    }
#endif
