#include "internal.h"

#include <_printf.h>
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

const int get_default_interface(char* const restrict interface_name, const uint32_t interface_name_length, const int sockfd) {
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);

    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len) < 0) {
        fprintf(stderr, "getsockname failed\n");
        return -1;
    }

    char local_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip))) {
        fprintf(stderr, "inet_ntop failed\n");
        return -1;
    }

    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "getifaddrs failed\n");
        return -1;
    }

    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char iface_ip[INET_ADDRSTRLEN];
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;

            inet_ntop(AF_INET, &sin->sin_addr, iface_ip, sizeof(iface_ip));

            if (strcmp(local_ip, iface_ip) == 0) {
                strncpy(interface_name, ifa->ifa_name, interface_name_length - 1);
                interface_name[interface_name_length - 1] = '\0';
                freeifaddrs(ifaddr);
                return 0;
            }
        }
    }

    freeifaddrs(ifaddr);
    
    fprintf(stderr, "No matching interface found for IP %s\n", local_ip);
    return -1;
}
