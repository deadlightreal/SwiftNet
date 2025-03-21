#include "../src/swift_net.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

SwiftNetClientConnection* con;

void handleMessagesFromServer(uint8_t* data) {
    printf("got message from server: %s\n", data);
}

void* handleClient() {
    mode = SWIFT_NET_CLIENT_MODE;

    con = SwiftNetCreateClient("192.168.1.64", 8080);
    SwiftNetSetMessageHandler(handleMessagesFromServer, con);

    for(uint8_t i = 0; i < 1; i++) {
        char* message = "hello server";

        SwiftNetAppendToPacket(con, message, strlen(message) + 1);

        SwiftNetSendPacket(con);

        usleep(5000);
    }

    return NULL;
}

SwiftNetServer* server;

void handleMessages(uint8_t* data) {
    printf("got message from client : %s\n", data);

    char* message = "hello client";

    SwiftNetAppendToPacket(server, message, strlen(message) + 1);

    SwiftNetSendPacket(server, server->lastClientAddrData);
}

void* handleServer() {
    mode = SWIFT_NET_SERVER_MODE;

    server = SwiftNetCreateServer("192.168.1.64", 8080);

    SwiftNetSetBufferSize(2048, server);

    SwiftNetSetMessageHandler(handleMessages, server);

    return NULL;
}

int main() {
    printf("starting\n");
    InitializeSwiftNet();

    pthread_t server_thread, client_thread;

    pthread_create(&server_thread, NULL, handleServer, NULL);
    pthread_create(&client_thread, NULL, handleClient, NULL);

    pthread_join(client_thread, NULL);
    pthread_join(server_thread, NULL);

    return 0;
}
