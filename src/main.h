#pragma once

#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>

#define MAX_CLIENT_CONNECTIONS 10
#define MAX_SERVERS 10

#define SWIFT_NET_CLIENT_MODE 1
#define SWIFT_NET_SERVER_MODE 2

#define unlikely(x) __builtin_expect((x), 0)
#define likely(x) __builtin_expect((x), 1)

__thread uint8_t mode;

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
    uint8_t* packetClientInfoPointer;
    uint8_t* packetDataCurrentPointer;
    uint8_t* packetDataStartPointer;
} SwiftNetClientConnection;

SwiftNetClientConnection SwiftNetClientConnections[MAX_CLIENT_CONNECTIONS] = {-1};

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
    uint8_t* packetClientInfoPointer;
    uint8_t* packetDataCurrentPointer;
    uint8_t* packetDataStartPointer;
} SwiftNetServer;

SwiftNetServer SwiftNetServers[MAX_SERVERS] = {-1};

#define SwiftNetClientCode(code) if(mode == SWIFT_NET_CLIENT_MODE) { code }
#define SwiftNetServerCode(code) if(mode == SWIFT_NET_SERVER_MODE) { code }

#ifndef RELEASE_MODE
    #define SwiftNetDebug(code) { code }
#else
    #define SwiftNetDebug(code)
#endif
