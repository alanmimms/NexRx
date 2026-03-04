cmake_minimum_required(VERSION 3.20)
project(NexRxTwin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

add_executable(nexrx-twin
    src/main.cpp
    src/signalgen.cpp
    src/control-handler.cpp
    src/transport/tcp-control-transport.cpp
    src/transport/udp-stream-transport.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CBOR REQUIRED libcbor)
target_link_libraries(nexrx-twin PRIVATE ${CBOR_LIBRARIES} pthread m)
