#define SWIFT_NET_CLIENT

#include "../../../src/swift_net.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

SwiftNetClientConnection* con;

void handleMessagesFromServer(uint8_t* data, SwiftNetPacketMetadata metadata) {
    printf("got message from server: %s\n", data);
}

int main() {
    swiftnet_initialize();

    printf("before creating client\n");
 
    con = swiftnet_create_client("192.168.1.64", 8080);

    printf("created client\n");

    swiftnet_set_buffer_size(1024, con);

    swiftnet_set_message_handler(handleMessagesFromServer, con);

    printf("before loop\n");

    for(uint8_t i = 0; ; i++) {
        printf("in loop\n");

        char* message = "hello server";

        swiftnet_append_to_packet(con, message, strlen(message) + 1);

        printf("before send\n");

        swiftnet_send_packet(con);

        printf("after send\n");

        swiftnet_clear_send_buffer(con);

        printf("message sent\n");

        usleep(500000);
    }

    swiftnet_cleanup_connection(con);

    return 0;
}
