#pragma once

#include "./initialize_client_socket.h"
#include <stdlib.h>
#include <stdint.h>

uint8_t* SwiftNetCreateDataArray(unsigned int size) {
    uint8_t* dataArray = (uint8_t*)malloc(size + sizeof(ClientInfo));

    return dataArray + sizeof(ClientInfo);
}

void SwiftNetDeleteDataArray(uint8_t* dataArray) {
    free(dataArray - sizeof(ClientInfo));
}
