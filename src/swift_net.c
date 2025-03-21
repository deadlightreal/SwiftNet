#include "swift_net.h"

__thread uint8_t mode = 0x00;

SwiftNetClientConnection SwiftNetClientConnections[MAX_CLIENT_CONNECTIONS] = {-1};
SwiftNetServer SwiftNetServers[MAX_SERVERS] = {-1};
