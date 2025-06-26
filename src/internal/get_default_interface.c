#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/route.h>
#include "../swift_net.h"

const int get_default_interface(char* const restrict interface_name) {
    FILE* const restrict fp = popen("route -n get default | grep interface | awk '{print $2}'", "r");
    if (unlikely(fp == NULL)) {
        fprintf(stderr, "popen failed\n");
        return -1;
    }

    fread(interface_name, 1, 32, fp);

    interface_name[strcspn(interface_name, "\n")] = 0;

    pclose(fp);

    return 0;
}
