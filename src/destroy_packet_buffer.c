#include "swift_net.h"
#include <stdlib.h>

void swiftnet_server_destroy_packet_buffer(const struct SwiftNetPacketBuffer* const packet) {
    free(packet->packet_buffer_start);
}

void swiftnet_client_destroy_packet_buffer(const struct SwiftNetPacketBuffer* const packet) {
    free(packet->packet_buffer_start);
}
