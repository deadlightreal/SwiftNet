cmake_minimum_required(VERSION 3.10)
project(SwiftNet C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

set(SOURCE_FILES
    initialize_swift_net.c
    swift_net.c
    send_packet.c
    handle_packets.c
    append_to_packet.c
    generic_functions.c
    initialize_client_socket.c
    initialize_server_socket.c
)

set(OBJ_DIR ${CMAKE_SOURCE_DIR}/../build)

add_library(swift_net STATIC ${SOURCE_FILES})

set_target_properties(swift_net PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../build
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../build)

file(GLOB OBJECT_FILES "${OBJ_DIR}/*.o")

execute_process(COMMAND ${CMAKE_COMMAND} -E remove_files ${OBJECT_FILES})
