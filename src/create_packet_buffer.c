#include "swift_net.h"
#include <stdint.h>
#include <stdlib.h>

static inline SwiftNetPacketBuffer create_packet_buffer(const uint32_t buffer_size) {
    uint8_t* restrict const mem = malloc(buffer_size + sizeof(SwiftNetPacketInfo));

    uint8_t* restrict const data_pointer = mem + sizeof(SwiftNetPacketInfo);

    return (SwiftNetPacketBuffer){
        .packet_buffer_start = mem,
        .packet_data_start = data_pointer,
        .packet_append_pointer = data_pointer 
    };
}

SwiftNetPacketBuffer swiftnet_server_create_packet_buffer(const uint32_t buffer_size) {
    return create_packet_buffer(buffer_size);
}

SwiftNetPacketBuffer swiftnet_client_create_packet_buffer(const uint32_t buffer_size) {
    return create_packet_buffer(buffer_size);
}
