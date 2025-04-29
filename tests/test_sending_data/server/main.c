#define SWIFT_NET_SERVER

#include "../../../src/swift_net.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

SwiftNetServer* server;

void handleMessages(uint8_t* data, SwiftNetPacketMetadata metadata) {
    printf("got a message : %s\n", data);

    char* message = "hello client";

    swiftnet_append_to_packet(server, message, strlen(message) + 1);

    printf("appended\n");

    swiftnet_send_packet(server, metadata.sender);

    swiftnet_clear_send_buffer(server);

    printf("sent\n");
}

int main() {
    swiftnet_initialize();

    server = swiftnet_create_server("192.168.1.64", 8080);

    swiftnet_set_buffer_size(2048, server);

    swiftnet_set_message_handler(handleMessages, server);

    while(1) {}

    swiftnet_cleanup_connection(server);

    return 0;
}
