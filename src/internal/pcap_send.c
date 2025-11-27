#include "internal.h"

int swiftnet_pcap_send(pcap_t *pcap, const u_char *data, int len) {
    if (pcap_inject(pcap, data, len) == -1) {
        PRINT_ERROR("inject error: %s", pcap_geterr(pcap));
        return -1;
    }

    return 0;
}
