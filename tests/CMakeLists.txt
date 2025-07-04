cmake_minimum_required(VERSION 3.15)
project(SocketProtocol C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

link_directories(${CMAKE_SOURCE_DIR}/../build/output)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -v")

file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.c")

foreach(src_file IN LISTS SRC_FILES)
    get_filename_component(exec_name ${src_file} NAME_WE)
    add_executable(${exec_name} ${src_file})
    target_link_libraries(${exec_name} swift_net)
endforeach()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(run_all_tests run_all_tests.c)
target_link_libraries(run_all_tests swift_net)
