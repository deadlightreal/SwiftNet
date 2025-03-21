#include "swift_net.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void SwiftNetSendPacket(void* connection, ...) {
    SwiftNetClientCode(
        printf("sending packet to server\n");

        SwiftNetClientConnection* con = (SwiftNetClientConnection *)connection;

        memcpy(con->packetClientInfoPointer, &con->clientInfo, sizeof(ClientInfo));

        int sent_packet = sendto(con->sockfd, con->packetClientInfoPointer, con->packetDataCurrentPointer - con->packetClientInfoPointer, 0, (const struct sockaddr *)&con->server_addr, sizeof(con->server_addr));
        if(sent_packet == -1) {
            printf("errno = %s\n", strerror(errno));
            exit(1);
        }

        con->packetDataCurrentPointer = con->packetDataStartPointer;
    )

    SwiftNetServerCode(
        va_list args;

        va_start(args, connection);

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
