#include "internal.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <pcap/pcap.h>

int setup_bpf_settings(const int bpf) {
    int one = 1;
    if (ioctl(bpf, BIOCIMMEDIATE, &one) == -1) {
        perror("BIOCIMMEDIATE");
        close(bpf);
        exit(1);
    }

    int hdr_complete = 1;
    if (ioctl(bpf, BIOCSHDRCMPLT, &hdr_complete) == -1) {
        perror("BIOCSHDRCMPLT");
        close(bpf);
        exit(1);
    }

    int promisc = 1;
    if (ioctl(bpf, BIOCPROMISC, &promisc) == -1) {
        perror("BIOCPROMISC");
        close(bpf);
        exit(1);
    }

    struct bpf_program fp;
    if (pcap_compile_nopcap(65535, DLT_EN10MB, &fp, "", 1, PCAP_NETMASK_UNKNOWN) != 0) {
        fprintf(stderr, "Failed to compile filter\n");
        close(bpf);
        exit(1);
    }

    if (ioctl(bpf, BIOCSETF, &fp) == -1) {
        perror("BIOCSETF");
        close(bpf);
        exit(1);
    }

    return 0;
}
