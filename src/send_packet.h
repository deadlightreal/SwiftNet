#pragma once

#include "./main.h"
#include <stdarg.h>
#include <string.h>

void SwiftNetSendPacket(void* connection, ...) {
    va_list args;

    va_start(args, connection);

    SwiftNetClientCode(
        SwiftNetClientConnection* con = (SwiftNetClientConnection *)connection;

        memcpy(con->packetClientInfoPointer, &con->clientInfo, sizeof(ClientInfo));

        sendto(con->sockfd, con->packetClientInfoPointer, con->packetDataCurrentPointer - con->packetClientInfoPointer, 0, (const struct sockaddr *)&con->server_addr, sizeof(con->server_addr));

        con->packetDataCurrentPointer = con->packetDataStartPointer;
    )

    SwiftNetServerCode(
        ClientAddrData clientAddr = va_arg(args, ClientAddrData);

        SwiftNetServer* server = (SwiftNetServer*)connection;

        ClientInfo connectionInfo;

        connectionInfo.destination_port = clientAddr.clientAddr.sin_port;
        connectionInfo.source_port = server->server_port;

        memcpy(server->packetClientInfoPointer, &connectionInfo, sizeof(ClientInfo));

        sendto(server->sockfd, server->packetClientInfoPointer, server->packetDataCurrentPointer - server->packetClientInfoPointer, 0, (const struct sockaddr *)&clientAddr.clientAddr, clientAddr.clientAddrLen);

        server->packetDataCurrentPointer = server->packetDataStartPointer;
    )
}
