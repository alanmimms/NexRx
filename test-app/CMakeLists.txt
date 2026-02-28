# NexRx Test & Calibration Suite - CMake Build System
cmake_minimum_required(VERSION 3.20)
project(NexRxTest
    VERSION 0.1.0
    DESCRIPTION "NexRx Test & Calibration Suite"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Transport code shared with main app
set(APP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app/src")
set(TWIN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../twin/src")

# External dependencies (FetchContent)
include(FetchContent)

# tinycbor
FetchContent_Declare(
    tinycbor
    GIT_REPOSITORY https://github.com/intel/tinycbor.git
    GIT_TAG v0.6.0
)
FetchContent_MakeAvailable(tinycbor)

# Build tinycbor as a static library
FetchContent_GetProperties(tinycbor)
if(NOT tinycbor_POPULATED)
    FetchContent_Populate(tinycbor)
endif()
add_library(tinycbor_lib STATIC
    ${tinycbor_SOURCE_DIR}/src/cborencoder.c
    ${tinycbor_SOURCE_DIR}/src/cborparser.c
    ${tinycbor_SOURCE_DIR}/src/cborerrorstrings.c
    ${tinycbor_SOURCE_DIR}/src/cborvalidation.c
)
target_include_directories(tinycbor_lib PUBLIC ${tinycbor_SOURCE_DIR}/src)

# Threads
find_package(Threads REQUIRED)

# Common sources for the test application
set(TEST_APP_SOURCES
    src/test-app.cpp
    src/test-engine.cpp
    src/remote-dev.cpp
    src/presel-cal.cpp
    src/iq-bal.cpp
    src/spur-chk.cpp
    src/pga-chk.cpp
    src/stream-chk.cpp
    src/select-chk.cpp
    
    # Shared with app/
    ${APP_SRC_DIR}/Socket.cpp
    ${APP_SRC_DIR}/TcpControlClient.cpp
    ${APP_SRC_DIR}/UdpStreamClient.cpp
    ${APP_SRC_DIR}/TwinConn.cpp
)

add_executable(nexrx-test ${TEST_APP_SOURCES})

target_include_directories(nexrx-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${APP_SRC_DIR}
    ${TWIN_SRC_DIR}
    ${tinycbor_SOURCE_DIR}/src
)

target_link_libraries(nexrx-test PRIVATE
    tinycbor_lib
    Threads::Threads
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(nexrx-test PRIVATE dl m)
endif()

# Summary
message(STATUS "NexRx Test Suite configured")
