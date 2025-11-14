#include "internal.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <pcap/pcap.h>

int setup_bpf_settings(const int bpf) {
    struct bpf_program fp;
    if (pcap_compile_nopcap(65535, DLT_EN10MB, &fp, "ip proto 253", 1, PCAP_NETMASK_UNKNOWN) != 0) {
        fprintf(stderr, "Failed to compile filter\n");
        return -1;
    }

    int im = 1;
    if (ioctl(bpf, BIOCIMMEDIATE, &im) < 0) {
        perror("BIOCIMMEDIATE");
        close(bpf);
        exit(EXIT_FAILURE);
    }

    int hdr_complete = 1;
    if (ioctl(bpf, BIOCSHDRCMPLT, &hdr_complete) < 0) {
        perror("BIOCSHDRCMPLT");
        close(bpf);
        exit(EXIT_FAILURE);
    }

    return 0;
}
