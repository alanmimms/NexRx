cmake_minimum_required(VERSION 3.20)
project(NexRxApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src ../twin/src)

add_executable(nexrx-app
    src/main.cpp
    src/twin-conn.cpp
    src/tcp-control-client.cpp
    src/udp-stream-client.cpp
    src/Socket.cpp
    src/AudioEngine.cpp
    src/BasebandFilter.cpp
    src/FontRenderer.cpp
    src/WaterfallRenderer.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CBOR REQUIRED libcbor)
target_link_libraries(nexrx-app PRIVATE ${CBOR_LIBRARIES} pthread m)
