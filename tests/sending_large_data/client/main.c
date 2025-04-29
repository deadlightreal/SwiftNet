#define SWIFT_NET_CLIENT

#include "../../../src/swift_net.h"
#include "../hash.h"
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

SwiftNetClientConnection* con;

void handler(uint8_t* data, SwiftNetPacketMetadata metadata) {

}

int main() {
    swiftnet_initialize();

    uint8_t* data = malloc(DATA_TO_SEND);

    for(size_t i = 0; i < DATA_TO_SEND; i++) {
        data[i] = rand();

        //printf("0x%02X ", data[i]);
    }

    unsigned long long hash = quickhash64(data, DATA_TO_SEND);

    con = swiftnet_create_client("192.168.1.64", 8080);

    swiftnet_set_message_handler(handler, con);

    swiftnet_set_buffer_size(DATA_TO_SEND, con);

    int zero = 0;

    swiftnet_append_to_packet(con, &zero, sizeof(int));

    swiftnet_send_packet(con);

    swiftnet_clear_send_buffer(con);

    printf("sent start\n");

    swiftnet_append_to_packet(con, data, DATA_TO_SEND);

    printf("appended\n");

    swiftnet_send_packet(con);

    printf("hash sent: %llx\n", hash);

    swiftnet_clear_send_buffer(con);

    free(data);

    swiftnet_cleanup_connection(con);

    return 0;
}
