#include "internal.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/bpf.h>
#include <net/ethernet.h>

int setup_bpf_settings(const int bpf) {
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
