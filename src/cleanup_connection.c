#include "internal/internal.h"
#include "swift_net.h"
#include <stdlib.h>


void swiftnet_cleanup_connection(const CONNECTION_TYPE* const restrict connection) {
    SwiftNetServerCode(
        free(connection->packet.packet_buffer_start);
    )

    SwiftNetClientCode(
        free(connection->packet.packet_buffer_start);
    )
}
