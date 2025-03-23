#pragma once

#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>

#define MAX_CLIENT_CONNECTIONS 0x0A
#define MAX_SERVERS 0x0A

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

typedef struct {
    uint16_t destination_port;
    uint16_t source_port;
} ClientInfo;

typedef struct {
    unsigned int packetDataLen;
    uint8_t* packetBufferStart;   // Start of the allocated buffer
    uint8_t* packetDataStart;     // Start of the stored data
    uint8_t* packetAppendPointer; // Current position to append new data
    uint8_t* packetReadPointer;
} Packet;

// Connection data
typedef struct {
    int sockfd;
    ClientInfo clientInfo;
    struct sockaddr_in server_addr;
    void (*packetHandler) (uint8_t* data);
    unsigned int bufferSize;
    pthread_t handlePacketsThread;
    Packet packet;
} SwiftNetClientConnection;

extern SwiftNetClientConnection SwiftNetClientConnections[MAX_CLIENT_CONNECTIONS];

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
