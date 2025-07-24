#include <_printf.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <stdio.h>
#include "swift_net.h"
#include "internal/internal.h"
#include <unistd.h>

SwiftNetDebug(
    SwiftNetDebugger debugger = {.flags = 0};
)

uint32_t maximum_transmission_unit = 0x00;
struct in_addr public_ip_address;

void swiftnet_initialize() {
    get_public_ip();

    int temp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (temp_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

    if (connect(temp_socket, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        fprintf(stderr, "Failed to connect temp socket\n");
        close(temp_socket);
        exit(EXIT_FAILURE);
    }

    char default_network_interface[128];

    const int got_default_interface = get_default_interface(default_network_interface, sizeof(default_network_interface), temp_socket);
    if(unlikely(got_default_interface != 0)) {
        close(temp_socket);
        fprintf(stderr, "Failed to get the default interface\n");
        exit(EXIT_FAILURE);
    }

    printf("default interface: %s\n", default_network_interface);

    maximum_transmission_unit = get_mtu(default_network_interface, temp_socket);
    if(unlikely(maximum_transmission_unit == 0)) {
        close(temp_socket);
        fprintf(stderr, "Failed to get the maximum transmission unit\n");
        exit(EXIT_FAILURE);
    }

    close(temp_socket);

    return;
}
