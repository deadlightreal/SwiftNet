#include "swift_net.h"
#include <stdlib.h>

void swiftnet_cleanup_connection(CONNECTION_TYPE* connection) {
    SwiftNetServerCode(
        free(connection->packet.packet_buffer_start);
    )

    SwiftNetClientCode(
        free(connection->packet.packet_buffer_start);
    )
}
