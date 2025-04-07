#define SWIFT_NET_SERVER

#include "../../../src/swift_net.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "../hash.h"
#include <stdlib.h>
#include <stdbool.h>

SwiftNetServer* server;

clock_t start;
bool started = false;

void handleMessages(uint8_t* data, ClientAddrData sender) {
    if(started == false) {
        printf("started\n");

        started = true;
        start = clock();

        return;
    }

    clock_t end = clock();
    double timeTaken = (double)(end - start) / CLOCKS_PER_SEC;

    printf("random byte: %d\n", data[5]);

    printf("time took to receive data: %f\n", timeTaken);

    unsigned long long hash = quickhash64(data, DATA_TO_SEND);

    printf("hash received: %llx\n", hash);

    exit(EXIT_SUCCESS);
}

int main() {
    InitializeSwiftNet();

    server = SwiftNetCreateServer("192.168.1.64", 8080);

    SwiftNetSetBufferSize(DATA_TO_SEND, server);

    SwiftNetSetMessageHandler(handleMessages, server);

    while(1) {}

    free(server->packet.packetBufferStart);

    return 0;
}
