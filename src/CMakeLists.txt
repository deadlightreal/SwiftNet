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
    cleanup.c
    internal/get_mtu.c
    internal/get_default_interface.c
    internal/datatype_allocator.c
)

set(OBJ_DIR ${CMAKE_SOURCE_DIR}/../build)

add_library(swift_net STATIC ${SOURCE_FILES})
target_compile_options(swift_net PRIVATE -fsanitize=address -fno-omit-frame-pointer -g -O0)
target_link_options(swift_net PRIVATE -g -O0 -fsanitize=address)

set_target_properties(swift_net PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OBJ_DIR}/output
    LIBRARY_OUTPUT_DIRECTORY ${OBJ_DIR}/output
    RUNTIME_OUTPUT_DIRECTORY ${OBJ_DIR}/output
    OUTPUT_NAME "swift_net"
    PUBLIC_HEADER "${CMAKE_SOURCE_DIR}/swift_net.h"
)

add_custom_target(clean_objects
    COMMAND ${CMAKE_COMMAND} -E rm -f ${OBJ_DIR}/output/*.o
    COMMENT "Cleaning up object files"
)

install(TARGETS swift_net
    EXPORT swiftnetTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

install(EXPORT swiftnetTargets
    FILE swiftnetTargets.cmake
    NAMESPACE swiftnet::
    DESTINATION lib/cmake/swiftnet
)

include(CMakePackageConfigHelpers)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cmake)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/swiftnetConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/swiftnetConfig.cmake
    INSTALL_DESTINATION lib/cmake/swiftnet
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/swiftnetConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/swiftnetConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/swiftnetConfigVersion.cmake
    DESTINATION lib/cmake/swiftnet
)
