#pragma once

#include "./initialize_client_socket.h"
#include <stdlib.h>
#include <stdint.h>

// Allocate an array for data, and return a pointer where user can write data.
// This function also allocates bytes client info
uint8_t* SwiftNetCreateDataArray(unsigned int size) {
    uint8_t* dataArray = (uint8_t*)malloc(size + sizeof(ClientInfo));

    return dataArray + sizeof(ClientInfo);
}

// Free the data array
void SwiftNetDeleteDataArray(uint8_t* dataArray) {
    free(dataArray - sizeof(ClientInfo));
}
