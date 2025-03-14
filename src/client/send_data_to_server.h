#pragma once

#include "./initialize_client_socket.h"

void SwiftNetSendDataToServer(uint8_t* data, unsigned int dataSize) {
    uint8_t* dataStartPointer = data - sizeof(ClientInfo);

    memcpy(dataStartPointer, &SwiftNetClientSocketData.clientInfo, sizeof(ClientInfo));

    if(sendto(SwiftNetClientSocketData.sockfd, dataStartPointer, dataSize + sizeof(ClientInfo), 0, (struct sockaddr *)&SwiftNetClientSocketData.server_addr, sizeof(SwiftNetClientSocketData.server_addr)) < 0) {
        perror("Failed to send packet!!\n");
    }
}
