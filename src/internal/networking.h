#pragma once

#include "../swift_net.h"
#include "internal.h"
#include <stdint.h>
#include <string.h>

#ifdef SWIFT_NET_BACKEND_PCAP

#include <sched.h>
#include <pcap/pcap.h>
    // Simple crc16 call with proper memory order
    #define HANDLE_CHECKSUM(buffer, size, network_data) \
        const uint32_t checksum = crc32(buffer, size); \
        memcpy(buffer + (network_data)->prepend_size + sizeof(struct ip) + offsetof(struct SwiftNetPacketInfo, checksum), &checksum, sizeof(checksum));

    #define GET_ADDR_TYPE(network_data) (network_data)->addr_type
    #define GET_PREPEND_SIZE(network_data) (network_data)->prepend_size

    #define HANDLE_PACKET_CONSTRUCTION(ip_header, packet_info, network_data, eth_hdr, buffer_size, buffer_name) \
        uint8_t buffer_name[buffer_size]; \
        if((network_data)->addr_type == DLT_NULL) { \
            uint32_t family = PF_INET; \
            memcpy(buffer_name, &family, sizeof(family)); \
            memcpy(buffer_name + sizeof(family), ip_header, sizeof(*ip_header)); \
            memcpy(buffer_name + sizeof(family) + sizeof(*ip_header), packet_info, sizeof(*packet_info)); \
        } else if((network_data)->addr_type == DLT_EN10MB){ \
            memcpy(buffer_name, eth_hdr, sizeof(*eth_hdr)); \
            memcpy(buffer_name + sizeof(*eth_hdr), ip_header, sizeof(*ip_header)); \
            memcpy(buffer_name + sizeof(*eth_hdr) + sizeof(*ip_header), packet_info, sizeof(*packet_info)); \
        }

    #define FREE_PACKET_CONSTRUCTION(buffer_name, network_data)

    extern pcap_t* swiftnet_pcap_open(const char* const restrict interface);
    extern int swiftnet_pcap_send(pcap_t *pcap, const uint8_t *data, int len);

    inline static struct SwiftNetNetworkData swiftnet_initialize_networking(const char* const restrict interface) {
        printf("Openning interface %s\n", interface);

        pcap_t* pcap = swiftnet_pcap_open(interface);
        if (unlikely(pcap == NULL)) {
            PRINT_ERROR("Failed to open pcap");

            struct SwiftNetNetworkData net_data_null;
            memset(&net_data_null, 0x00, sizeof(net_data_null));

            return net_data_null;
        }

        const uint8_t addr_type = pcap_datalink(pcap);
        
        struct SwiftNetNetworkData network_data = {
            .pcap = pcap,
            .addr_type = addr_type,
            .prepend_size = PACKET_PREPEND_SIZE(addr_type)
        };

        return network_data;
    }

    #define SWIFTNET_BREAK_RECEIVER_LOOP(network_data) \
        pcap_breakloop((network_data)->pcap)

    #define SWIFTNET_LOOP_PACKETS(network_data, listener) \
        pcap_loop((network_data)->pcap, 0, pcap_packet_handle, (void*)listener);

    #define SWIFTNET_CLOSE_CONNECTION(network_data) \
        pcap_close((network_data)->pcap);

    #define SWIFTNET_SEND_PACKET(network_data, buffer, len) \
	while(swiftnet_pcap_send((network_data)->pcap, buffer, len) == -2) { \
        usleep(2000); \
    }

    #define SWIFTNET_SEND_INTERNAL_PACKET(network_data, buffer, len) \
	while(swiftnet_pcap_send((network_data)->pcap, buffer, len) == -2) { \
        usleep(2000); \
    }
#elif defined(SWIFT_NET_BACKEND_DPDK)
    #include <rte_ethdev.h>

    #define HANDLE_CHECKSUM(buffer, size, network_data) \
        const uint32_t checksum = crc32(buffer, size); \
        memcpy(buffer + (network_data)->prepend_size + sizeof(struct ip) + offsetof(struct SwiftNetPacketInfo, checksum), &checksum, sizeof(checksum));

    #define GET_ADDR_TYPE(network_data) (network_data)->addr_type
    #define GET_PREPEND_SIZE(network_data) (network_data)->prepend_size

    #define HANDLE_PACKET_CONSTRUCTION(ip_header, packet_info, network_data, eth_hdr, buffer_size, buffer_name) \
        struct rte_mbuf* buffer_name##_internal_mem_buf = rte_pktmbuf_alloc((network_data)->mem_pool); \
        uint8_t* restrict const buffer_name = (uint8_t*)rte_pktmbuf_append(buffer_name##_internal_mem_buf, buffer_size); \
        buffer_name##_internal_mem_buf->data_len = buffer_size; \
        buffer_name##_internal_mem_buf->pkt_len = buffer_size; \
        if((network_data)->addr_type == 0) { \
            memcpy(buffer_name, ip_header, sizeof(*ip_header)); \
            memcpy(buffer_name + sizeof(*ip_header), packet_info, sizeof(*packet_info)); \
        } else { \
            memcpy(buffer_name, eth_hdr, sizeof(*eth_hdr)); \
            memcpy(buffer_name + sizeof(*eth_hdr), ip_header, sizeof(*ip_header)); \
            memcpy(buffer_name + sizeof(*eth_hdr) + sizeof(*ip_header), packet_info, sizeof(*packet_info)); \
        }

    #define SWIFTNET_LOOP_PACKETS(network_data, listener) \
        while(1) { \
            struct rte_mbuf* buffers[DPDK_BURST_SIZE]; \
            uint16_t nb_rx = rte_eth_rx_burst((network_data)->port, 0, buffers, DPDK_BURST_SIZE); \
            if (likely(nb_rx > 0)) { \
                for (uint16_t i = 0; i < nb_rx; i++) { \
                    struct rte_mbuf* buffer = buffers[i]; \
                    uint8_t* const raw_data_ptr = rte_pktmbuf_mtod(buffer, uint8_t*); \
                    dpdk_packet_handle(listener, buffer, raw_data_ptr); \
                    rte_pktmbuf_free(buffer); \
                } \
            } else { \
                rte_pause(); \
            } \
        }

    #define FREE_PACKET_CONSTRUCTION(buffer_name, network_data) \
        rte_pktmbuf_free(buffer_name##_internal_mem_buf);

    void swiftnet_dpdk_open_port(const uint16_t port, struct SwiftNetNetworkData* restrict const);
    extern int swiftnet_dpdk_send(const uint16_t port, struct rte_mbuf* buf);

    inline static struct SwiftNetNetworkData swiftnet_initialize_networking(const char* const restrict interface) {
        struct SwiftNetNetworkData network_data;
        uint16_t interface_port;

        interface_port = rte_eth_dev_get_port_by_name(interface, &interface_port);

        swiftnet_dpdk_open_port(interface_port, &network_data);

        network_data = (struct SwiftNetNetworkData){
            .port = interface_port,
            .addr_type = 1,
            .prepend_size = sizeof(struct ether_header)
        };

        return network_data;
    }


    #define SWIFTNET_CLOSE_CONNECTION(network_data) \
        rte_eth_dev_stop((network_data)->port);

    #define SWIFTNET_SEND_INTERNAL_PACKET(network_data, buffer, len) \
        swiftnet_dpdk_send((network_data)->port, buffer##_internal_mem_buf);

    #define SWIFTNET_SEND_PACKET(network_data, buffer, len) \
        swiftnet_dpdk_send((network_data)->port, buffer##_internal_mem_buf);
#endif
