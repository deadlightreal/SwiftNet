#include "internal.h"

pcap_t* swiftnet_pcap_open(const char* interface) {
    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_t *p = pcap_open_live(
        interface,
        65535,
        0,
        25,
        errbuf
    );

    if (!p) {
        fprintf(stderr, "pcap_open_live failed: %s\n", errbuf);
        return NULL;
    }

    struct bpf_program fp;
    char filter_exp[] = "ip proto 253";
    bpf_u_int32 net = 0;

    if (pcap_compile(p, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(p));
        exit(EXIT_FAILURE);
    }

    if (pcap_setfilter(p, &fp) != 0) {
        fprintf(stderr, "Unable to set filter: %s\n", pcap_geterr(p));
    }

    return p;
}
