#include "swift_net.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

// These functions send the data from the packet buffer to the designated client or server.

static inline void NullCheckConnection(void* ptr) {
    if(unlikely(ptr == NULL)) {
        fprintf(stderr, "Error: First argument ( Client | Server ) in send packet function is NULL!!\n");
        exit(EXIT_FAILURE);
    }
}

static inline PacketSending* GetEmptyPacketSending(PacketSending* packet_sending_array, const uint16_t size) {
    for(uint16_t i = 0; i < size; i++) {
        PacketSending* current_packet_sending = &packet_sending_array[i];

        if(current_packet_sending->packet_id == 0x00) {
            return current_packet_sending;
        }
    }

    return NULL;
}

SwiftNetClientCode(
static inline void WaitForNextChunkRequest(bool* requested_next_chunk) {
    while(*requested_next_chunk == false) {
    }

    *requested_next_chunk = false;
}

void SwiftNetSendPacket(SwiftNetClientConnection* client) {
    SwiftNetErrorCheck(
        NullCheckConnection(client);
    )

    uint16_t packet_id = rand();

    unsigned int mtu = maximum_transmission_unit >= client->maximum_transmission_unit ? client->maximum_transmission_unit : maximum_transmission_unit;

    PacketInfo packetInfo = {};
    packetInfo.client_info = client->clientInfo;
    packetInfo.packet_length = client->packet.packetAppendPointer - client->packet.packetDataStart;
    packetInfo.packet_id = packet_id;
    packetInfo.chunk_size = mtu;
    packetInfo.packet_type = PACKET_TYPE_MESSAGE;

    memcpy(client->packet.packetBufferStart, &packetInfo, sizeof(PacketInfo));

    if(packetInfo.packet_length > mtu) {
        PacketSending* empty_packet_sending = GetEmptyPacketSending(client->packets_sending, MAX_PACKETS_SENDING);
        if(unlikely(empty_packet_sending == NULL)) {
            fprintf(stderr, "Failed to send a packet: exceeded maximum amount of sending packets at the same time\n");
            return;
        }
    
        empty_packet_sending->packet_id = packet_id;
        empty_packet_sending->requested_next_chunk = false;

        uint8_t buffer[mtu];

        memcpy(buffer, &packetInfo, sizeof(PacketInfo));

        for(unsigned int currentOffset = 0; ; currentOffset += mtu - sizeof(PacketInfo)) {
            printf("sending packet\n");
            if(currentOffset + mtu > packetInfo.packet_length) {
                printf("finished sending packet\n");

                unsigned int bytesToSend = packetInfo.packet_length - currentOffset;

                memcpy(&buffer[sizeof(PacketInfo)], client->packet.packetDataStart + currentOffset, bytesToSend);
                sendto(client->sockfd, buffer, bytesToSend + sizeof(PacketInfo), 0, (const struct sockaddr *)&client->server_addr, sizeof(client->server_addr));
                
                break;
            } else {
                memcpy(&buffer[sizeof(PacketInfo)], client->packet.packetDataStart + currentOffset, mtu - sizeof(PacketInfo));
                sendto(client->sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr *)&client->server_addr, sizeof(client->server_addr));
            }


            WaitForNextChunkRequest(&empty_packet_sending->requested_next_chunk);
        }
    } else {
        memcpy(client->packet.packetBufferStart, &packetInfo, sizeof(PacketInfo));
        
        sendto(client->sockfd, client->packet.packetBufferStart, client->packet.packetAppendPointer - client->packet.packetBufferStart, 0, (const struct sockaddr *)&client->server_addr, sizeof(client->server_addr));

        client->packet.packetAppendPointer = client->packet.packetDataStart;
        client->packet.packetReadPointer = client->packet.packetDataStart;
    }
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

    ClientInfo clientInfo;

    clientInfo.destination_port = clientAddress->clientAddr.sin_port;
    clientInfo.source_port = server->server_port;

    uint16_t packet_id = rand();

    unsigned int mtu = maximum_transmission_unit >= clientAddress->maximum_transmission_unit ? clientAddress->maximum_transmission_unit : maximum_transmission_unit;

    PacketInfo packetInfo = {};
    packetInfo.client_info = clientInfo;
    packetInfo.packet_length = server->packet.packetAppendPointer - server->packet.packetDataStart;
    packetInfo.packet_id = packet_id;
    packetInfo.chunk_size = mtu;
    packetInfo.packet_type = PACKET_TYPE_MESSAGE;

    memcpy(server->packet.packetBufferStart, &packetInfo, sizeof(PacketInfo));

    if(packetInfo.packet_length > mtu) {
        // Send data in multiple chunks
        uint8_t buffer[mtu];

        const unsigned int data_size = mtu - sizeof(PacketInfo);

        for(unsigned int offset = 0; offset < packetInfo.packet_length; offset += data_size) {
            memcpy(&buffer[sizeof(PacketInfo)], &server->packet.packetDataStart[offset], data_size);
        }
    }

    sendto(server->sockfd, server->packet.packetBufferStart, server->packet.packetAppendPointer - server->packet.packetBufferStart, 0, (const struct sockaddr *)&clientAddress->clientAddr, clientAddress->clientAddrLen);

    server->packet.packetAppendPointer = server->packet.packetDataStart;
    server->packet.packetReadPointer = server->packet.packetDataStart;
}
)
