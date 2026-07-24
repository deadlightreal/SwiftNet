set(CMAKE_BUILD_TYPE Debug)

add_compile_definitions(
    SWIFT_NET_MEMORY_USAGE=5
)

set(SWIFT_NET_INTERNAL_TESTING ON)
set(SANITIZER "none")
set(BACKEND "pcap")
