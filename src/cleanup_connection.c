#include "internal/internal.h"
#include "swift_net.h"
#include <stdlib.h>

void swiftnet_client_cleanup(const SwiftNetClientConnection* const restrict client) {
    free(client->packet.packet_buffer_start);
}

void swiftnet_server_cleanup(const SwiftNetServer* const restrict server) {
    free(server->packet.packet_buffer_start);
}
