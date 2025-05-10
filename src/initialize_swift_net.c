#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "swift_net.h"
#include "internal/internal.h"

unsigned int maximum_transmission_unit = 0x00;

PacketQueue packet_queue;

void swiftnet_initialize() {
    char default_network_interface[32];

    const int got_default_interface = GetDefaultInterface(default_network_interface);
    if(unlikely(got_default_interface != 0)) {
        fprintf(stderr, "Failed to get the default interface\n");
        exit(EXIT_FAILURE);
    }

    maximum_transmission_unit = GetMtu(default_network_interface);
if(unlikely(maximum_transmission_unit == 0)) {
        fprintf(stderr, "Failed to get the maximum transmission unit\n");
        exit(EXIT_FAILURE);
    }

    return;
}
