#define SWIFT_NET_CLIENT

#include "../../../src/swift_net.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

SwiftNetClientConnection* con;

void handleMessagesFromServer(uint8_t* data) {
    printf("got message from server: %s\n", data);
}

int main() {
    InitializeSwiftNet();

    con = SwiftNetCreateClient("192.168.1.64", 8080);

    SwiftNetSetBufferSize(1024, con);

    SwiftNetSetMessageHandler(handleMessagesFromServer, con);

    for(uint8_t i = 0; ; i++) {
        char* message = "hello server";

        SwiftNetAppendToPacket(con, message, strlen(message) + 1);

        SwiftNetSendPacket(con);

        printf("message sent\n");

        usleep(500000);
    }

    return 0;
}
