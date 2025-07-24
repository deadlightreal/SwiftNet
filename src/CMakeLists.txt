cmake_minimum_required(VERSION 3.10)
project(SwiftNet C)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
    initialize_swift_net.c
    swift_net.c
    send_packet.c
    handle_packets.c
    append_to_packet.c
    generic_functions.c
    initialize_client_socket.c
    initialize_server_socket.c
    cleanup_connection.c
    process_packets.c
    execute_packet_callback.c
    create_packet_buffer.c
    add_debug_flags.c
    destroy_packet_buffer.c
    internal/get_mtu.c
    internal/get_default_interface.c
    internal/get_public_ip.c
)

set(OBJ_DIR ${CMAKE_SOURCE_DIR}/../build)

# SwiftNet Client Library
add_library(swift_net STATIC ${SOURCE_FILES})
set_target_properties(swift_net PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OBJ_DIR}/output
    OUTPUT_NAME "swift_net"
    RUNTIME_OUTPUT_DIRECTORY ${OBJ_DIR}/output
    LIBRARY_OUTPUT_DIRECTORY ${OBJ_DIR}/output
)

add_custom_target(clean_objects
    COMMAND ${CMAKE_COMMAND} -E rm -f ${OBJ_DIR}/output/*.o
    COMMENT "Cleaning up object files"
)
