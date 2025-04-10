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

#define PACKET_TYPE_MESSAGE 0x01
#define PACKET_TYPE_SEND_NEXT_CHUNK 0x02
#define PACKET_TYPE_REQUEST_INFORMATION 0x03

#define PACKET_INFO_ID_NONE 0xFFFF

#define DEFAULT_DATA_CHUNK_SIZE 0x1000

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

extern unsigned int maximum_transmission_unit;

typedef struct {
    uint16_t destination_port;
    uint16_t source_port;
} ClientInfo;

typedef struct {
    unsigned int packet_length;
    ClientInfo client_info;
    uint16_t packet_id;
    uint8_t packet_type;
    unsigned int chunk_size;
} PacketInfo;

typedef struct {
    unsigned int maximum_transmission_unit;
} ServerInformation;

typedef struct {
    unsigned int packetDataLen;
    uint8_t* packetBufferStart;   // Start of the allocated buffer
    uint8_t* packetDataStart;     // Start of the stored data
    uint8_t* packetAppendPointer; // Current position to append new data
    uint8_t* packetReadPointer;
} Packet;

typedef struct {
    uint8_t* packetDataStart;
    uint8_t* packetCurrentPointer;
    PacketInfo packetInfo;
    in_addr_t clientAddress;
} TransferClient;

// Connection data
typedef struct {
    int sockfd;
    ClientInfo clientInfo;
    struct sockaddr_in server_addr;
    void (*packetHandler) (uint8_t* data);
    unsigned int bufferSize;
    pthread_t handlePacketsThread;
    Packet packet;
    unsigned int dataChunkSize;
    unsigned int maximum_transmission_unit;
} SwiftNetClientConnection;

extern SwiftNetClientConnection SwiftNetClientConnections[MAX_CLIENT_CONNECTIONS];
extern bool SwiftNetServerRequestedNextChunk;

typedef struct {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
} ClientAddrData;

typedef struct {
    int sockfd;
    uint16_t server_port;
    unsigned int bufferSize;
    void (*packetHandler)(uint8_t* data, ClientAddrData sender);
    pthread_t handlePacketsThread;
    Packet packet;
    unsigned int dataChunkSize;
    TransferClient transferClients[MAX_TRANSFER_CLIENTS];
} SwiftNetServer;

extern SwiftNetServer SwiftNetServers[MAX_SERVERS];

typedef struct {
    void* connection;
    uint8_t mode;
} SwiftNetHandlePacketsArgs;

#ifndef SWIFT_NET_DISABLE_ERROR_CHECKING
    #define SwiftNetErrorCheck(code) code
#else
    #define SwiftNetErrorCheck(code)
#endif

#ifdef SWIFT_NET_CLIENT
    #define SwiftNetClientCode(code) code
#else
    #define SwiftNetClientCode(code)
#endif

#ifdef SWIFT_NET_SERVER
    #define SwiftNetServerCode(code) code
#else
    #define SwiftNetServerCode(code)
#endif

SwiftNetServerCode(
    void SwiftNetSendPacket(SwiftNetServer* server, ClientAddrData* clientAddress);
    void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data, ClientAddrData sender), SwiftNetServer* connection);
    void* SwiftNetHandlePackets(void* serverVoid);
    void SwiftNetSetBufferSize(unsigned int newBufferSize, SwiftNetServer* server);
    void SwiftNetAppendToPacket(SwiftNetServer* server, void* data, unsigned int dataSize);
    void SwiftNetReadFromPacket(SwiftNetServer* server, void* ptr, unsigned int size);
    static inline void SwiftNetReadStringFromPacket(SwiftNetServer* server, void* ptr) {
        SwiftNetReadFromPacket(server, ptr, strlen((char*)server->packet.packetReadPointer) + 1);
    })

SwiftNetClientCode(
    void SwiftNetSendPacket(SwiftNetClientConnection* client);
    void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data), SwiftNetClientConnection* connection);
    void* SwiftNetHandlePackets(void* clientVoid);
    void SwiftNetSetBufferSize(unsigned int newBufferSize, SwiftNetClientConnection* client);
    void SwiftNetAppendToPacket(SwiftNetClientConnection* client, void* data, unsigned int dataSize);
    void SwiftNetReadFromPacket(SwiftNetClientConnection* client, void* ptr, unsigned int size);
    static inline void SwiftNetReadStringFromPacket(SwiftNetClientConnection* client, void* ptr) {
        SwiftNetReadFromPacket(client, ptr, strlen((char*)client->packet.packetReadPointer) + 1);
    })

SwiftNetServer* SwiftNetCreateServer(char* ip_address, uint16_t port);
SwiftNetClientConnection* SwiftNetCreateClient(char* ip_address, int port);
void InitializeSwiftNet();
