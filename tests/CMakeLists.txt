cmake_minimum_required(VERSION 3.15)
project(SwiftNetTests C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

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

link_directories(${CMAKE_SOURCE_DIR}/../build/output)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")

file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.c")

foreach(src_file IN LISTS SRC_FILES)
    get_filename_component(exec_name ${src_file} NAME_WE)
    add_executable(${exec_name} ${src_file})
    target_compile_options(${exec_name} PRIVATE)
    target_link_libraries(${exec_name} swiftnet pcap)
endforeach()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(run_all_tests run_all_tests.c)
target_link_libraries(run_all_tests swiftnet pcap)
target_link_options(run_all_tests PRIVATE -g -O0)
