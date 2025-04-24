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
SwiftNetClientConnection* swiftnet_create_client(char* ip_address, int port) {
    SwiftNetClientConnection* emptyConnection = NULL;
    for(uint8_t i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        SwiftNetClientConnection* currentConnection = &SwiftNetClientConnections[i];
        if(currentConnection->sockfd != -1) {
            continue;
        }

        emptyConnection = currentConnection;

        break;
    }

    SwiftNetErrorCheck(
        if(unlikely(emptyConnection == NULL)) {
            perror("Failed to get an empty connection\n");
            exit(EXIT_FAILURE);
        }
    )

    emptyConnection->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(unlikely(emptyConnection->sockfd < 0)) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
    
    uint16_t clientPort = rand();

    emptyConnection->port_info.destination_port = port;
    emptyConnection->port_info.source_port = clientPort;
    emptyConnection->packet_handler = NULL;

    // Base buffer size
    emptyConnection->buffer_size = 0x400; // 1024

    // Allocate memory for the packet buffer
    uint8_t* dataPointer = (uint8_t*)malloc(emptyConnection->buffer_size + sizeof(SwiftNetPacketInfo));
    if(unlikely(dataPointer == NULL)) {
        perror("Failed to allocate memory for packet data\n");
        exit(EXIT_FAILURE);
    }

    emptyConnection->packet.packet_buffer_start = dataPointer;
    emptyConnection->packet.packet_data_start = dataPointer + sizeof(SwiftNetPacketInfo);
    

    emptyConnection->packet.packet_append_pointer= emptyConnection->packet.packet_data_start;

    memset(&emptyConnection->server_addr, 0, sizeof(emptyConnection->server_addr));
    emptyConnection->server_addr.sin_family = AF_INET;
    emptyConnection->server_addr.sin_port = htons(port);
    emptyConnection->server_addr.sin_addr.s_addr = inet_addr(ip_address);

    // Request the server information, and proccess it
    uint8_t request_information_data[sizeof(SwiftNetPacketInfo)];

    SwiftNetPacketInfo packetInfo;
    packetInfo.port_info = emptyConnection->port_info;
    packetInfo.packet_length = 0;
    packetInfo.packet_id = rand();
    packetInfo.packet_type = PACKET_TYPE_REQUEST_INFORMATION;

    memcpy(request_information_data, &packetInfo, sizeof(SwiftNetPacketInfo));

    memset(emptyConnection->pending_messages, 0, MAX_PENDING_MESSAGES * sizeof(PendingMessage));
    memset(emptyConnection->packets_sending, 0, MAX_PACKETS_SENDING * sizeof(SwiftNetPacketSending));

    /*int flags = fcntl(emptyConnection->sockfd, F_GETFL, 0);  // Get current flags
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }

    int new_flags = flags |= O_NONBLOCK;  // Add O_NONBLOCK to the flags
    if (fcntl(emptyConnection->sockfd, F_SETFL, new_flags) == -1) {  // Set the socket to non-blocking mode
        perror("fcntl F_SETFL");
        exit(EXIT_FAILURE);
    }*/

    uint8_t server_information_buffer[sizeof(SwiftNetPacketInfo) + sizeof(SwiftNetServerInformation) + sizeof(struct ip)];

    socklen_t server_addr_len = sizeof(emptyConnection->server_addr);

    bool received = false;

    while(received == false) {

        sendto(emptyConnection->sockfd, request_information_data, sizeof(request_information_data), 0, (struct sockaddr *)&emptyConnection->server_addr, sizeof(emptyConnection->server_addr));

        for(uint8_t i = 0; i < 10; i++) {
            int bytes_received = recvfrom(emptyConnection->sockfd, server_information_buffer, sizeof(server_information_buffer), 0, (struct sockaddr *)&emptyConnection->server_addr, &server_addr_len);

            SwiftNetPacketInfo* packetInfo = (SwiftNetPacketInfo *)&server_information_buffer[sizeof(struct ip)];

            if(packetInfo->port_info.destination_port != emptyConnection->port_info.source_port || packetInfo->port_info.source_port != emptyConnection->port_info.destination_port) {
                continue;
            }
            
            if(bytes_received != 0) {
                received = true;
                break;
            }
            
            usleep(1000000);
        }
    }

    SwiftNetServerInformation* server_information = (SwiftNetServerInformation*)&server_information_buffer[sizeof(SwiftNetPacketInfo) + sizeof(struct ip)];

    emptyConnection->maximum_transmission_unit = server_information->maximum_transmission_unit;

    /*if (fcntl(emptyConnection->sockfd, F_SETFL, flags) == -1) {  // Set the socket back to blocking mode
        perror("fcntl F_SETFL");
        exit(EXIT_FAILURE);
    }*/
 
    pthread_create(&emptyConnection->handle_packets_thread, NULL, swiftnet_handle_packets, emptyConnection);

    return emptyConnection;
}
