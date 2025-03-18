#pragma once

#include "./main.h"
#include <stdlib.h>
#include <string.h>

void SwiftNetAppendToPacket(void* connection, void* data, unsigned int dataSize) {
    SwiftNetServerCode(
        SwiftNetServer* Server = (SwiftNetServer*)connection;

        memcpy(Server->packetDataCurrentPointer, data, dataSize);

        Server->packetDataCurrentPointer += dataSize;
    )

    SwiftNetClientCode(
        SwiftNetClientConnection* Connection = (SwiftNetClientConnection*)connection;

        memcpy(Connection->packetDataCurrentPointer, data, dataSize);

        Connection->packetDataCurrentPointer += dataSize;
    )
}
