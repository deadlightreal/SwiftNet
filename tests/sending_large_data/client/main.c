#define SWIFT_NET_CLIENT

#include "../../../src/swift_net.h"
#include "../hash.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

SwiftNetClientConnection* con;

void handler(uint8_t* data) {

}

int main() {
    InitializeSwiftNet();

    uint8_t* data = malloc(DATA_TO_SEND);

    for(size_t i = 0; i < DATA_TO_SEND; i++) {
        data[i] = rand();

        //printf("0x%02X ", data[i]);
    }

    unsigned long long hash = quickhash64(data, DATA_TO_SEND);

    con = SwiftNetCreateClient("192.168.1.64", 8080);

    SwiftNetSetMessageHandler(handler, con);

    SwiftNetSetBufferSize(DATA_TO_SEND, con);

    int zero = 0;

    SwiftNetAppendToPacket(con, &zero, sizeof(int));

    SwiftNetSendPacket(con);

    SwiftNetAppendToPacket(con, data, DATA_TO_SEND);

    SwiftNetSendPacket(con);

    printf("random byte: %d\n", data[5]);

    printf("hash sent: %llx\n", hash);
    printf("hash sent: %llx\n", quickhash64(data, DATA_TO_SEND));

    free(data);

    return 0;
}
