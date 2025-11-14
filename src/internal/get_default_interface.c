#include "internal.h"

#include <stdio.h>
#include <net/if_dl.h>
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

int get_default_interface_and_mac(char* restrict interface_name, uint32_t interface_name_length, uint8_t mac_out[6], int sockfd) {
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

    struct ifaddrs *match = NULL;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char iface_ip[INET_ADDRSTRLEN];
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;

            if (!inet_ntop(AF_INET, &sin->sin_addr, iface_ip, sizeof(iface_ip)))
                continue;

            if (strcmp(local_ip, iface_ip) == 0) {
                match = ifa;
                break;
            }
        }
    }

    if (!match) {
        fprintf(stderr, "Fatal Error: No matching interface found for IP %s\n", local_ip);
        freeifaddrs(ifaddr);
        return -1;
    }

    strncpy(interface_name, match->ifa_name, interface_name_length - 1);
    interface_name[interface_name_length - 1] = '\0';

    int mac_found = 0;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        if (strcmp(ifa->ifa_name, interface_name) == 0 &&
            ifa->ifa_addr->sa_family == AF_LINK) {

            struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;

            if (sdl->sdl_alen == 6) {
                memcpy(mac_out, LLADDR(sdl), 6);
                mac_found = 1;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    if (!mac_found) {
        fprintf(stderr, "Fatal Error: No MAC address found for interface %s\n", interface_name);
        return -1;
    }

    return 0;
}
