#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "swift_net.h"
#include <fcntl.h>

// Create the socket, and set client and server info
SwiftNetClientConnection* swiftnet_create_client(const char* const restrict ip_address, const int port) {
    SwiftNetClientConnection* restrict empty_connection = NULL;
    for(uint8_t i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        SwiftNetClientConnection* const restrict currentConnection = &SwiftNetClientConnections[i];
        if(currentConnection->sockfd != -1) {
            continue;
        }

        empty_connection = currentConnection;

        break;
    }

    SwiftNetErrorCheck(
        if(unlikely(empty_connection == NULL)) {
            perror("Failed to get an empty connection\n");
            exit(EXIT_FAILURE);
        }
    )

    empty_connection->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(unlikely(empty_connection->sockfd < 0)) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
    
    const uint16_t clientPort = rand();

    empty_connection->port_info.destination_port = port;
    empty_connection->port_info.source_port = clientPort;
    empty_connection->packet_handler = NULL;

    // Base buffer size
    empty_connection->buffer_size = 0x400; // 1024

    // Allocate memory for the packet buffer
    uint8_t* restrict const buffer_pointer = malloc(empty_connection->buffer_size + sizeof(SwiftNetPacketInfo));
    if(unlikely(buffer_pointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    uint8_t* restrict const data_pointer = buffer_pointer + sizeof(SwiftNetPacketInfo);

    empty_connection->packet.packet_buffer_start = buffer_pointer;
    empty_connection->packet.packet_data_start = data_pointer;
    empty_connection->packet.packet_append_pointer = data_pointer;

    memset(&empty_connection->server_addr, 0, sizeof(empty_connection->server_addr));
    empty_connection->server_addr.sin_family = AF_INET;
    empty_connection->server_addr.sin_port = htons(port);
    empty_connection->server_addr.sin_addr.s_addr = inet_addr(ip_address);

    // Request the server information, and proccess it
    uint8_t request_information_data[sizeof(SwiftNetPacketInfo)];

    const SwiftNetPacketInfo packetInfo = {
        .port_info = empty_connection->port_info,
        .packet_length = 0x00,
        .packet_id = rand(),
        .packet_type = PACKET_TYPE_REQUEST_INFORMATION
    };

    memcpy(request_information_data, &packetInfo, sizeof(SwiftNetPacketInfo));

    memset(empty_connection->pending_messages, 0x00, MAX_PENDING_MESSAGES * sizeof(SwiftNetPendingMessage));
    memset((void *)empty_connection->packets_sending, 0x00, MAX_PACKETS_SENDING * sizeof(SwiftNetPacketSending));

    uint8_t server_information_buffer[sizeof(SwiftNetPacketInfo) + sizeof(SwiftNetServerInformation) + sizeof(struct ip)];

    while(1) {
        sendto(empty_connection->sockfd, request_information_data, sizeof(request_information_data), 0, (struct sockaddr *)&empty_connection->server_addr, sizeof(empty_connection->server_addr));

        for(uint8_t i = 0; i < 10; i++) {
            const int bytes_received = recvfrom(empty_connection->sockfd, server_information_buffer, sizeof(server_information_buffer), 0x00, NULL, NULL);

            const SwiftNetPacketInfo* const restrict packetInfo = (SwiftNetPacketInfo *)&server_information_buffer[sizeof(struct ip)];

            if(packetInfo->port_info.destination_port != empty_connection->port_info.source_port || packetInfo->port_info.source_port != empty_connection->port_info.destination_port) {
                continue;
            }
            
            if(bytes_received != 0) {
                goto after_getting_server_info;
                break;
            }
            
            usleep(1000000);
        }

    after_getting_server_info:
        break;

    }

    const SwiftNetServerInformation* const restrict server_information = (SwiftNetServerInformation*)&server_information_buffer[sizeof(SwiftNetPacketInfo) + sizeof(struct ip)];

    empty_connection->maximum_transmission_unit = server_information->maximum_transmission_unit;
 
    pthread_create(&empty_connection->handle_packets_thread, NULL, swiftnet_handle_packets, empty_connection);

    return empty_connection;
}
