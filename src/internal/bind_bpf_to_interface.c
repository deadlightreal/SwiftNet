#include "internal.h"
#include <net/if.h>
#include <net/bpf.h>
#include <sys/ioctl.h>
#include <unistd.h>

int bind_bpf_to_interface(const int bpf) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, default_network_interface, sizeof(ifr.ifr_name)-1);
    if (ioctl(bpf, BIOCSETIF, &ifr) < 0) {
        fprintf(stderr, "Failed: ioctl BIOCSETIF\n");
        close(bpf);
        return 1;
    }

    return 0;
}
