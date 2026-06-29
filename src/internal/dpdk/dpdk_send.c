#include "../internal.h"
#include <rte_ethdev.h>

int swiftnet_dpdk_send(const uint16_t port, struct rte_mbuf* buf) {
    rte_eth_tx_burst(port, 0, &buf, 1);

    return 0;
}
