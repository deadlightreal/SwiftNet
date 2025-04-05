#include "swift_net.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// These functions send the data from the packet buffer to the designated client or server.

static inline void NullCheckConnection(void* ptr) {
    if(unlikely(ptr == NULL)) {
        fprintf(stderr, "Error: First argument ( Client | Server ) in send packet function is NULL!!\n");
        exit(EXIT_FAILURE);
    }
}

SwiftNetClientCode(
void SwiftNetSendPacket(SwiftNetClientConnection* client) {
    SwiftNetErrorCheck(
        NullCheckConnection(client);
    )

    PacketInfo packetInfo = {};
    packetInfo.client_info = client->clientInfo;
    packetInfo.packet_length = client->packet.packetAppendPointer - client->packet.packetDataStart;

    memcpy(client->packet.packetBufferStart, &packetInfo, sizeof(PacketInfo));

    printf("network data sent: ");
    /*for(uint8_t i = 0; i < packetInfo.packet_length + sizeof(PacketInfo); i++) {
        printf("0x%02X", client->packet.packetBufferStart[i]);
    }*/
    printf("\n");

    printf("sent %d bytes\n", packetInfo.packet_length);

    sendto(client->sockfd, client->packet.packetBufferStart, client->packet.packetAppendPointer - client->packet.packetBufferStart, 0, (const struct sockaddr *)&client->server_addr, sizeof(client->server_addr));

    client->packet.packetAppendPointer = client->packet.packetDataStart;
    client->packet.packetReadPointer = client->packet.packetDataStart;
}
)

SwiftNetServerCode(
void SwiftNetSendPacket(SwiftNetServer* server, ClientAddrData* clientAddress) {
    SwiftNetErrorCheck(
        NullCheckConnection(server);

        if(unlikely(clientAddress == NULL)) {
            fprintf(stderr, "Error: Pointer to client address (Second argument) is NULL in SendPacket function\nWhen in server mode passing client address is required\n");
            exit(EXIT_FAILURE);
        }
    )

    ClientInfo connectionInfo;

    connectionInfo.destination_port = clientAddress->clientAddr.sin_port;
    connectionInfo.source_port = server->server_port;

    memcpy(server->packet.packetBufferStart, &connectionInfo, sizeof(ClientInfo));

    sendto(server->sockfd, server->packet.packetBufferStart, server->packet.packetAppendPointer - server->packet.packetBufferStart, 0, (const struct sockaddr *)&clientAddress->clientAddr, clientAddress->clientAddrLen);

    server->packet.packetAppendPointer = server->packet.packetDataStart;
    server->packet.packetReadPointer = server->packet.packetDataStart;
}
)
