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
    if(!started) {
        started = true;
        start = clock();
        printf("started\n");
        return;
    }

    clock_t end = clock();

    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("time taken to process message: %f\n", time_taken);

    unsigned long long hash = quickhash64(data, DATA_TO_SEND);

    printf("hash got: %llx\n", hash);

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
