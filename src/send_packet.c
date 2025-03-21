#include "swift_net.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// These functions send the data from the packet buffer to the designated client or server.

static inline void SendPacketToServer(SwiftNetClientConnection* client) {
    memcpy(client->packetBufferStart, &client->clientInfo, sizeof(ClientInfo));

    sendto(client->sockfd, client->packetBufferStart, client->packetAppendPointer - client->packetBufferStart, 0, (const struct sockaddr *)&client->server_addr, sizeof(client->server_addr));

    client->packetAppendPointer = client->packetDataStart;
}

static inline void SendPacketToClient(SwiftNetServer* server, ClientAddrData* clientAddress) {
    SwiftNetDebug(
        if(unlikely(clientAddress == NULL)) {
            fprintf(stderr, "Error: Pointer to client address (Second argument) is NULL in SendPacket function\nWhen in server mode passing client address is required\n");
            exit(EXIT_FAILURE);
        }
    )

    ClientInfo connectionInfo;

    connectionInfo.destination_port = clientAddress->clientAddr.sin_port;
    connectionInfo.source_port = server->server_port;

    memcpy(server->packetBufferStart, &connectionInfo, sizeof(ClientInfo));

    sendto(server->sockfd, server->packetBufferStart, server->packetAppendPointer - server->packetBufferStart, 0, (const struct sockaddr *)&clientAddress->clientAddr, clientAddress->clientAddrLen);

    server->packetAppendPointer = server->packetDataStart;
}

void SwiftNetSendPacket(void* connection, void* clientAddress) {
    if(unlikely(connection == NULL)) {
        fprintf(stderr, "Error: First argument ( Client | Server ) in send packet function is NULL!!\n");
        exit(EXIT_FAILURE);
    }

    SwiftNetClientCode(
        SendPacketToServer(connection);
    )

    SwiftNetServerCode(
        SendPacketToClient(connection, clientAddress);
    )
}
