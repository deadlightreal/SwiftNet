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

void handleMessages(uint8_t* data, SwiftNetPacketMetadata metadata) {
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
    swiftnet_initialize();

    server = swiftnet_create_server("192.168.1.64", 8080);

    swiftnet_set_buffer_size(DATA_TO_SEND, server);

    swiftnet_set_message_handler(handleMessages, server);

    while(1) {}

    swiftnet_cleanup_connection(server);

    return 0;
}
