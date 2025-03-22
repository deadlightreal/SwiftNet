#pragma once

#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>

#define MAX_CLIENT_CONNECTIONS 0x0A
#define MAX_SERVERS 0x0A

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

typedef struct {
    uint16_t destination_port;
    uint16_t source_port;
} ClientInfo;

// Connection data
typedef struct {
    int sockfd;
    ClientInfo clientInfo;
    struct sockaddr_in server_addr;
    void (*packetHandler) (uint8_t* data);
    unsigned int bufferSize;
    pthread_t handlePacketsThread;
    uint8_t* packetBufferStart;   // Start of the allocated buffer
    uint8_t* packetDataStart;     // Start of the stored data
    uint8_t* packetAppendPointer; // Current position to append new data
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
    uint8_t* packetBufferStart;   // Start of the allocated buffer
    uint8_t* packetDataStart;     // Start of the stored data
    uint8_t* packetAppendPointer; // Current position to append new data
} SwiftNetServer;

extern SwiftNetServer SwiftNetServers[MAX_SERVERS];

typedef struct {
    void* connection;
    uint8_t mode;
} SwiftNetHandlePacketsArgs;

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

#ifndef RELEASE_MODE
    #define SwiftNetDebug(code) { code }
#else
    #define SwiftNetDebug(code)
#endif

SwiftNetServerCode(
    void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data, ClientAddrData sender), SwiftNetServer* connection);
)

SwiftNetClientCode(
    void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data), SwiftNetClientConnection* connection);
)

void SwiftNetSendPacket(void* connection, void* clientAddress);
void* SwiftNetHandlePackets(void* voidArgs);
SwiftNetServer* SwiftNetCreateServer(char* ip_address, uint16_t port);
SwiftNetClientConnection* SwiftNetCreateClient(char* ip_address, int port);
void SwiftNetAppendToPacket(void* connection, void* data, unsigned int dataSize);
void SwiftNetSetBufferSize(unsigned int newBufferSize, void* connection);
void InitializeSwiftNet();
