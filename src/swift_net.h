#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <netinet/ip.h>
#include <stdbool.h>

#define MAX_CLIENT_CONNECTIONS 0x0A
#define MAX_SERVERS 0x0A
#define MAX_TRANSFER_CLIENTS 0x0A
#define MAX_PENDING_MESSAGES 0x0A
#define MAX_PACKETS_SENDING 0x0A

#define PACKET_TYPE_MESSAGE 0x01
#define PACKET_TYPE_SEND_NEXT_CHUNK 0x02
#define PACKET_TYPE_REQUEST_INFORMATION 0x03

#define PACKET_INFO_ID_NONE 0xFFFF

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

#ifndef SWIFT_NET_DISABLE_ERROR_CHECKING
    #define SwiftNetErrorCheck(code) code
#else
    #define SwiftNetErrorCheck(code)
#endif

#ifdef SWIFT_NET_CLIENT
    #define CONNECTION_TYPE SwiftNetClientConnection
    #define SwiftNetClientCode(code) code
#else
    #define SwiftNetClientCode(code)
#endif

#ifdef SWIFT_NET_SERVER
    #define CONNECTION_TYPE SwiftNetServer
    #define EXTRA_REQUEST_NEXT_CHUNK_ARG , SwiftNetClientAddrData target
    #define SEND_PACKET_EXTRA_ARG , SwiftNetClientAddrData client_address
    #define SwiftNetServerCode(code) code
#else   
    #define EXTRA_REQUEST_NEXT_CHUNK_ARG
    #define SEND_PACKET_EXTRA_ARG
    #define SwiftNetServerCode(code)
#endif

extern unsigned int maximum_transmission_unit;

typedef struct {
    uint16_t destination_port;
    uint16_t source_port;
} SwiftNetPortInfo;

typedef struct {
    struct sockaddr_in client_address;
    socklen_t client_address_length;
    unsigned int maximum_transmission_unit;
} SwiftNetClientAddrData;

typedef struct {
    unsigned int data_length;

    SwiftNetServerCode(
        SwiftNetClientAddrData sender;
    )
} SwiftNetPacketMetadata;

typedef struct {
    unsigned int packet_length;
    SwiftNetPortInfo port_info;
    uint16_t packet_id;
    uint8_t packet_type;
    unsigned int chunk_size;
} SwiftNetPacketInfo;

typedef struct {
    unsigned int maximum_transmission_unit;
} SwiftNetServerInformation;

typedef struct {
    uint16_t packet_id;
    bool requested_next_chunk;
} SwiftNetPacketSending;

typedef struct {
    uint8_t* packet_buffer_start;   // Start of the allocated buffer
    uint8_t* packet_data_start;     // Start of the stored data
    uint8_t* packet_append_pointer; // Current position to append new data
} SwiftNetPacket;

typedef struct {
    uint8_t* packet_data_start;
    uint8_t* packet_current_pointer;
    SwiftNetPacketInfo packet_info;
    in_addr_t client_address;
} SwiftNetTransferClient;

typedef struct {
    uint8_t* packet_data_start;
    uint8_t* packet_current_pointer;
    SwiftNetPacketInfo packet_info;
} PendingMessage;

// Connection data
typedef struct {
    int sockfd;
    SwiftNetPortInfo port_info;
    struct sockaddr_in server_addr;
    void (*packet_handler) (uint8_t*, SwiftNetPacketMetadata);
    unsigned int buffer_size;
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    SwiftNetPacket packet;
    unsigned int maximum_transmission_unit;
    PendingMessage pending_messages[MAX_PENDING_MESSAGES];
    SwiftNetPacketSending packets_sending[MAX_PACKETS_SENDING];
    uint8_t* current_read_pointer;
} SwiftNetClientConnection;

extern SwiftNetClientConnection SwiftNetClientConnections[MAX_CLIENT_CONNECTIONS];

typedef struct {
    int sockfd;
    uint16_t server_port;
    unsigned int buffer_size;
    void (*packet_handler)(uint8_t*, SwiftNetPacketMetadata);
    pthread_t handle_packets_thread;
    pthread_t process_packets_thread;
    SwiftNetPacket packet;
    SwiftNetTransferClient transfer_clients[MAX_TRANSFER_CLIENTS];
    SwiftNetPacketSending packets_sending[MAX_PACKETS_SENDING];
    uint8_t* current_read_pointer;
} SwiftNetServer;

extern SwiftNetServer SwiftNetServers[MAX_SERVERS];

void swiftnet_set_message_handler(void (*handler)(uint8_t*, SwiftNetPacketMetadata), CONNECTION_TYPE* connection);
void* swiftnet_handle_packets(void* connection);
void swiftnet_set_buffer_size(unsigned int new_buffer_size, CONNECTION_TYPE* connection);
void swiftnet_append_to_packet(CONNECTION_TYPE* connection, void* data, unsigned int data_size);
void swiftnet_send_packet(CONNECTION_TYPE* connection EXTRA_REQUEST_NEXT_CHUNK_ARG);
SwiftNetServer* swiftnet_create_server(char* ip_address, uint16_t port);
SwiftNetClientConnection* swiftnet_create_client(char* ip_address, int port);
void swiftnet_initialize();
void swiftnet_cleanup_connection(CONNECTION_TYPE* connection);

// ----------------
// INLINE FUNCTIONS
// ----------------

static inline void swiftnet_clear_send_buffer(CONNECTION_TYPE* connection) {
    connection->packet.packet_append_pointer = connection->packet.packet_data_start;
}

static inline void swiftnet_append_uint8(uint8_t num, CONNECTION_TYPE* connection) {
    *connection->packet.packet_append_pointer = num;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline void swiftnet_append_uint16(uint16_t num, CONNECTION_TYPE* connection) {
    connection->packet.packet_append_pointer[0] = num & 0xFF;
    connection->packet.packet_append_pointer[1] = (num >> 8) & 0xFF;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline void swiftnet_append_uint32(uint32_t num, CONNECTION_TYPE* connection) {
    connection->packet.packet_append_pointer[0] = num & 0xFF;
    connection->packet.packet_append_pointer[1] = (num >> 8) & 0xFF;
    connection->packet.packet_append_pointer[2] = (num >> 16) & 0xFF;
    connection->packet.packet_append_pointer[3] = (num >> 24) & 0xFF;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline void swiftnet_append_uint64(uint64_t num, CONNECTION_TYPE* connection) {
    connection->packet.packet_append_pointer[0] = num & 0xFF;
    connection->packet.packet_append_pointer[1] = (num >> 8) & 0xFF;
    connection->packet.packet_append_pointer[2] = (num >> 16) & 0xFF;
    connection->packet.packet_append_pointer[3] = (num >> 24) & 0xFF;
    connection->packet.packet_append_pointer[4] = (num >> 32) & 0xFF;
    connection->packet.packet_append_pointer[5] = (num >> 40) & 0xFF;
    connection->packet.packet_append_pointer[6] = (num >> 48) & 0xFF;
    connection->packet.packet_append_pointer[7] = (num >> 56) & 0xFF;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline void swiftnet_append_int8(int8_t num, CONNECTION_TYPE* connection) {
    *connection->packet.packet_append_pointer = num;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline void swiftnet_append_int16(int16_t num, CONNECTION_TYPE* connection) {
    connection->packet.packet_append_pointer[0] = num & 0xFF;
    connection->packet.packet_append_pointer[1] = (num >> 8) & 0xFF;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline void swiftnet_append_int32(int32_t num, CONNECTION_TYPE* connection) {
    connection->packet.packet_append_pointer[0] = num & 0xFF;
    connection->packet.packet_append_pointer[1] = (num >> 8) & 0xFF;
    connection->packet.packet_append_pointer[2] = (num >> 16) & 0xFF;
    connection->packet.packet_append_pointer[3] = (num >> 24) & 0xFF;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline void swiftnet_append_int64(int64_t num, CONNECTION_TYPE* connection) {
    connection->packet.packet_append_pointer[0] = num & 0xFF;
    connection->packet.packet_append_pointer[1] = (num >> 8) & 0xFF;
    connection->packet.packet_append_pointer[2] = (num >> 16) & 0xFF;
    connection->packet.packet_append_pointer[3] = (num >> 24) & 0xFF;
    connection->packet.packet_append_pointer[4] = (num >> 32) & 0xFF;
    connection->packet.packet_append_pointer[5] = (num >> 40) & 0xFF;
    connection->packet.packet_append_pointer[6] = (num >> 48) & 0xFF;
    connection->packet.packet_append_pointer[7] = (num >> 56) & 0xFF;

    connection->packet.packet_append_pointer += sizeof(num);
}

static inline uint8_t swiftnet_read_uint8(CONNECTION_TYPE* connection) {
    uint8_t result = *connection->current_read_pointer;

    connection->current_read_pointer += sizeof(result);

    return result;
}

static inline uint16_t swiftnet_read_uint16(CONNECTION_TYPE* connection) {
    uint16_t result = connection->current_read_pointer[0] | connection->current_read_pointer[1] << 8;

    connection->current_read_pointer += sizeof(result);

    return result;
}

static inline uint32_t swiftnet_read_uint32(CONNECTION_TYPE* connection) {
    uint32_t result = connection->current_read_pointer[0] | connection->current_read_pointer[1] << 8 | connection->current_read_pointer[2] << 16 | connection->current_read_pointer[3] << 24;

    connection->current_read_pointer += sizeof(result);

    return result;
}

static inline uint64_t swiftnet_read_uint64(CONNECTION_TYPE* connection) {
    uint64_t result = 
        ((uint64_t)connection->current_read_pointer[0])       |
        ((uint64_t)connection->current_read_pointer[1] << 8)  |
        ((uint64_t)connection->current_read_pointer[2] << 16) |
        ((uint64_t)connection->current_read_pointer[3] << 24) |
        ((uint64_t)connection->current_read_pointer[4] << 32) |
        ((uint64_t)connection->current_read_pointer[5] << 40) |
        ((uint64_t)connection->current_read_pointer[6] << 48) |
        ((uint64_t)connection->current_read_pointer[7] << 56);

    connection->current_read_pointer += sizeof(result);

    return result;
}

static inline int8_t swiftnet_read_int8(CONNECTION_TYPE* connection) {
    int8_t result = *connection->current_read_pointer;

    connection->current_read_pointer += sizeof(result);

    return result;
}

static inline int16_t swiftnet_read_int16(CONNECTION_TYPE* connection) {
    int16_t result = connection->current_read_pointer[0] | connection->current_read_pointer[1] << 8;

    connection->current_read_pointer += sizeof(result);

    return result;
}

static inline int32_t swiftnet_read_int32(CONNECTION_TYPE* connection) {
    int32_t result = connection->current_read_pointer[0] | connection->current_read_pointer[1] << 8 | connection->current_read_pointer[2] << 16 | connection->current_read_pointer[3] << 24;

    connection->current_read_pointer += sizeof(result);

    return result;
}

static inline int64_t swiftnet_read_int64(CONNECTION_TYPE* connection) {
    int64_t result = 
        ((uint64_t)connection->current_read_pointer[0])       |
        ((uint64_t)connection->current_read_pointer[1] << 8)  |
        ((uint64_t)connection->current_read_pointer[2] << 16) |
        ((uint64_t)connection->current_read_pointer[3] << 24) |
        ((uint64_t)connection->current_read_pointer[4] << 32) |
        ((uint64_t)connection->current_read_pointer[5] << 40) |
        ((uint64_t)connection->current_read_pointer[6] << 48) |
        ((uint64_t)connection->current_read_pointer[7] << 56);

    connection->current_read_pointer += sizeof(result);

    return result;
}
