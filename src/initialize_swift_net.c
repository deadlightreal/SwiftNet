#include <stdlib.h>
#include <time.h>
#include "swift_net.h"
#include <stdio.h>

void InitializeSwiftNet() {
    SwiftNetServerCode(
        printf("initializing server\n");
    )

    SwiftNetClientCode(
        printf("initializing client\n");
    )

    return;
}
