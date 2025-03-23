#define SWIFT_NET_SERVER

#include "../../../src/swift_net.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

SwiftNetServer* server;

void handleMessages(uint8_t* data, ClientAddrData sender) {
    char message_from_sender[100];
    SwiftNetReadStringFromPacket(server, message_from_sender);

    printf("got message from client : %s\n", message_from_sender);

    char* message = "hello client";

    SwiftNetAppendToPacket(server, message, strlen(message) + 1);

    SwiftNetSendPacket(server, &sender);
}

int main() {
    InitializeSwiftNet();

    server = SwiftNetCreateServer("192.168.1.64", 8080);

    SwiftNetSetBufferSize(2048, server);

    SwiftNetSetMessageHandler(handleMessages, server);

    while(1) {}

    return 0;
}
