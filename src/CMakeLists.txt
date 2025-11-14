cmake_minimum_required(VERSION 3.10)
project(swiftnet C CXX)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
    initialize_swift_net.c
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
    make_response.c
    make_request.c
    internal/get_mtu.c
    internal/get_default_interface_and_mac.c
    internal/datatype_allocator.c
    internal/datatype_vector.c
    internal/pcap_open.c
    internal/pcap_send.c
    internal/check_existing_listener.c
)

if(SANITIZER STREQUAL "address")
    message(STATUS "Using AddressSanitizer")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
elseif(SANITIZER STREQUAL "thread")
    message(STATUS "Using ThreadSanitizer")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
elseif(SANITIZER STREQUAL "undefined")
    message(STATUS "Using UndefinedBehaviorSanitizer")
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
else()
    message(STATUS "No sanitizer enabled")
endif()

add_library(swiftnet STATIC ${SOURCE_FILES})
target_compile_options(swiftnet PRIVATE -lpcap -O0)
target_link_options(swiftnet PRIVATE -lpcap -O0)

set_target_properties(swiftnet PROPERTIES
    OUTPUT_NAME "swiftnet"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
    PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/swift_net.h"
)

add_custom_target(clean_objects
    COMMAND ${CMAKE_COMMAND} -E rm -f ${OBJ_DIR}/output/*.o
    COMMENT "Cleaning up object files"
)

install(TARGETS swiftnet
    EXPORT swiftnetTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
)

install(EXPORT swiftnetTargets
    FILE swiftnetTargets.cmake
    NAMESPACE swiftnet::
    DESTINATION share/deadlightreal-swiftnet
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
)

include(CMakePackageConfigHelpers)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cmake)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/deadlightreal-swiftnetConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/deadlightreal-swiftnetConfig.cmake
    INSTALL_DESTINATION share/deadlightreal-swiftnet
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/deadlightreal-swiftnetConfigVersion.cmake
    VERSION 0.1.0
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/deadlightreal-swiftnetConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/deadlightreal-swiftnetConfigVersion.cmake
    DESTINATION share/deadlightreal-swiftnet
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
)
