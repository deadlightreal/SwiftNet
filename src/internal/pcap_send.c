#include "internal.h"

int swiftnet_pcap_send(pcap_t *pcap, const u_char *data, int len) {
    if (pcap_inject(pcap, data, len) == -1) {
        fprintf(stderr, "inject error: %s\n", pcap_geterr(pcap));
        return -1;
    }

    return 0;
}
