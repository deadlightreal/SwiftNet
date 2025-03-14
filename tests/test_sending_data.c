#include "../src/client/include_client.h"
#include "../src/server/include_server.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

void* handleClient() {
    SwiftNetCreateClient("192.168.1.64", 8080);

    uint8_t* dataPointer = SwiftNetCreateDataArray(0);

    printf("client info size: %lu\n", sizeof(ClientInfo));

    for(uint8_t i = 0; i < 10; i++) {
        printf("sending data\n");

        SwiftNetSendDataToServer(dataPointer, 0);

        usleep(50000);
    }

    SwiftNetDeleteDataArray(dataPointer);

    return NULL;
}

void handleMessages(uint8_t* data) {
    printf("Got message from server\n");
}

void* handleServer() {
    SwiftNetCreateServer("192.168.1.64", 8080);

    SwiftNetSetBufferSize(2048);

    SwiftNetSetMessageHandler(handleMessages);

    return NULL;
}

int main() {
    pthread_t server_thread, client_thread;

    pthread_create(&server_thread, NULL, handleServer, NULL);
    pthread_create(&client_thread, NULL, handleClient, NULL);

    pthread_join(client_thread, NULL);
    pthread_join(server_thread, NULL);

    return 0;
}
