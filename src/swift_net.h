#pragma once

#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>

#define MAX_CLIENT_CONNECTIONS 0x0A
#define MAX_SERVERS 0x0A

#define SWIFT_NET_CLIENT_MODE 0x01
#define SWIFT_NET_SERVER_MODE 0x02

#define SWIFT_NET_SERVER SwiftNetMode = SWIFT_NET_SERVER_MODE;
#define SWIFT_NET_CLIENT SwiftNetMode = SWIFT_NET_CLIENT_MODE;

#define unlikely(x) __builtin_expect((x), 0x00)
#define likely(x) __builtin_expect((x), 0x01)

extern __thread uint8_t SwiftNetMode;

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
    void (*packetHandler)(uint8_t* data);
    pthread_t handlePacketsThread;
    ClientAddrData lastClientAddrData;
    uint8_t* packetBufferStart;   // Start of the allocated buffer
    uint8_t* packetDataStart;     // Start of the stored data
    uint8_t* packetAppendPointer; // Current position to append new data
} SwiftNetServer;

extern SwiftNetServer SwiftNetServers[MAX_SERVERS];

typedef struct {
    void* connection;
    uint8_t mode;
} SwiftNetHandlePacketsArgs;

#define SwiftNetClientCode(code) if(SwiftNetMode == SWIFT_NET_CLIENT_MODE) { code }
#define SwiftNetServerCode(code) if(SwiftNetMode == SWIFT_NET_SERVER_MODE) { code }

#ifndef RELEASE_MODE
    #define SwiftNetDebug(code) { code }
#else
    #define SwiftNetDebug(code)
#endif

void SwiftNetSendPacket(void* connection, void* clientAddress);
void* SwiftNetHandlePackets(void* voidArgs);
SwiftNetServer* SwiftNetCreateServer(char* ip_address, uint16_t port);
SwiftNetClientConnection* SwiftNetCreateClient(char* ip_address, int port);
void SwiftNetAppendToPacket(void* connection, void* data, unsigned int dataSize);
void SwiftNetSetMessageHandler(void(*handler)(uint8_t* data), void* connection);
void SwiftNetSetBufferSize(unsigned int newBufferSize, void* connection);
void InitializeSwiftNet();
