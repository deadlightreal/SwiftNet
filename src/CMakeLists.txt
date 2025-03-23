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
    read_from_packet.c
)

set(OBJ_DIR ${CMAKE_SOURCE_DIR}/../build)

# SwiftNet Client Library
add_library(swift_net_client STATIC ${SOURCE_FILES})
target_compile_definitions(swift_net_client PRIVATE SWIFT_NET_CLIENT)
set_target_properties(swift_net_client PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OBJ_DIR}/client
    OUTPUT_NAME "swift_net_client"
    RUNTIME_OUTPUT_DIRECTORY ${OBJ_DIR}/client
    LIBRARY_OUTPUT_DIRECTORY ${OBJ_DIR}/client
)

# SwiftNet Server Library
add_library(swift_net_server STATIC ${SOURCE_FILES})
target_compile_definitions(swift_net_server PRIVATE SWIFT_NET_SERVER)
set_target_properties(swift_net_server PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OBJ_DIR}/server
    OUTPUT_NAME "swift_net_server"
    RUNTIME_OUTPUT_DIRECTORY ${OBJ_DIR}/server
    LIBRARY_OUTPUT_DIRECTORY ${OBJ_DIR}/server
)

add_custom_target(clean_objects
    COMMAND ${CMAKE_COMMAND} -E rm -f ${OBJ_DIR}/client/*.o ${OBJ_DIR}/server/*.o
    COMMENT "Cleaning up object files"
)
