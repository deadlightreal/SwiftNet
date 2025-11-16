#include <net/bpf.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/bpf.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "internal.h"

int get_bpf_device() {
    char dev[] = "/dev/bpf0";
    int bpf = -1;
    for (int i = 0; i < 256; ++i) {
        dev[8] = '0' + (i % 10);
        if (i >= 10) dev[7] = '0' + ((i/10)%10);
        bpf = open(dev, O_RDWR);
        if (bpf >= 0) break;
        if (errno == EBUSY || errno == EACCES) continue;
    }
    if (bpf < 0) {
        fprintf(stderr, "Failed to open open /dev/bpf*");
        exit(EXIT_FAILURE);
    }

    printf("got bpf: %d\n", bpf);

    return bpf;
}
