#include "../../../src/swift_net.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

SwiftNetServer* server;

void handleMessages(uint8_t* data) {
    printf("got message from client : %s\n", data);

    char* message = "hello client";

    SwiftNetAppendToPacket(server, message, strlen(message) + 1);

    SwiftNetSendPacket(server, &server->lastClientAddrData);
}

int main() {
    InitializeSwiftNet();

    server = SwiftNetCreateServer("192.168.1.64", 8080);

    SwiftNetSetBufferSize(2048, server);

    SwiftNetSetMessageHandler(handleMessages, server);

    while(1) {}

    return 0;
}
