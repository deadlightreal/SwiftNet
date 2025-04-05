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
        data[i] = rand() % 256;
    }

    unsigned long long hash = quickhash64(data, DATA_TO_SEND);

    printf("hash sent: %llx\n", hash);

    con = SwiftNetCreateClient("192.168.1.64", 8080);

    SwiftNetSetMessageHandler(handler, con);

    SwiftNetSetBufferSize(DATA_TO_SEND, con);

    int zero = 0;

    SwiftNetAppendToPacket(con, &zero, sizeof(int));

    SwiftNetSendPacket(con);

    SwiftNetAppendToPacket(con, data, DATA_TO_SEND);

    SwiftNetSendPacket(con);

    free(data);

    return 0;
}
