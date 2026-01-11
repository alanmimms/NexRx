# NexRx App - Native UI Application
# Phase 2: Custom Lua GUI with SDL2/OpenGL

cmake_minimum_required(VERSION 3.20)
project(NexRxApp
    VERSION 0.1.0
    DESCRIPTION "NexRx Native UI Application"
    LANGUAGES CXX C
)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static link MSVC runtime on Windows (no vcruntime DLL dependency)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Build type defaults to RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All platforms use TCP/UDP to connect to twin (signalgen or STM32)
set(NEXRX_REMOTE_TWIN TRUE)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

# Check if using vcpkg (set automatically by vcpkg toolchain)
if(DEFINED VCPKG_TARGET_TRIPLET OR NEXRX_WINDOWS)
    # vcpkg mode (Windows/macOS)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)

    # SDL2 libraries
    set(SDL2_INCLUDE_DIRS "")  # SDL2::SDL2 handles includes
    if(WIN32)
        set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main)
    else()
        set(SDL2_LIBRARIES SDL2::SDL2)
    endif()

    # Lua 5.4 from vcpkg
    find_package(Lua REQUIRED)
    set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
    set(LUA_LIBS ${LUA_LIBRARIES})
    message(STATUS "Using Lua (vcpkg): ${LUA_LIBRARIES}")
else()
    # Native build (Linux): use pkg-config
    find_package(PkgConfig REQUIRED)

    # Use regular Lua 5.4 everywhere (simpler, consistent across platforms)
    pkg_check_modules(LUA54 REQUIRED lua5.4)
    set(LUA_INCLUDE_DIRS ${LUA54_INCLUDE_DIRS})
    set(LUA_LIBS ${LUA54_LIBRARIES})
    message(STATUS "Using Lua (pkg-config): ${LUA_LIBS}")

    pkg_check_modules(SDL2 REQUIRED sdl2)
    find_package(OpenGL REQUIRED)
endif()

# sol2 is header-only, we'll fetch it
# Lua is compiled as C, not C++
add_compile_definitions(
    SOL_ALL_SAFETIES_OFF=1
    SOL_USING_CXX_LUA=0
    SOL_LUAJIT=0
)

include(FetchContent)

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG develop
)

# tinycbor - lightweight CBOR encoder/decoder for UDP transport
FetchContent_Declare(
    tinycbor
    GIT_REPOSITORY https://github.com/intel/tinycbor.git
    GIT_TAG v0.6.0
)

FetchContent_MakeAvailable(sol2)

# Build tinycbor for CBOR encoding/decoding
FetchContent_GetProperties(tinycbor)
if(NOT tinycbor_POPULATED)
    FetchContent_Populate(tinycbor)
endif()

# Build tinycbor as a static library (it doesn't have CMake support)
add_library(tinycbor STATIC
    ${tinycbor_SOURCE_DIR}/src/cborencoder.c
    ${tinycbor_SOURCE_DIR}/src/cborparser.c
    ${tinycbor_SOURCE_DIR}/src/cborerrorstrings.c
    ${tinycbor_SOURCE_DIR}/src/cborvalidation.c
)
target_include_directories(tinycbor PUBLIC ${tinycbor_SOURCE_DIR}/src)

# Threads
find_package(Threads REQUIRED)

# Twin transport headers (for IQFrame, etc.)
set(TWIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../twin")

# -----------------------------------------------------------------------------
# SetBox library - tag-based configuration engine
# -----------------------------------------------------------------------------
add_library(setbox STATIC
    src/setbox/SetBox.cpp
)

target_include_directories(setbox PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LUA_INCLUDE_DIRS}
)

target_link_libraries(setbox PUBLIC
    ${LUA_LIBS}
    sol2::sol2
)

if(MSVC)
    target_compile_options(setbox PRIVATE /W4)
else()
    target_compile_options(setbox PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
    )
endif()

# -----------------------------------------------------------------------------
# Test executable for SetBox engine
# -----------------------------------------------------------------------------
add_executable(setbox_test src/setbox_test.cpp)

target_link_libraries(setbox_test PRIVATE
    setbox
    Threads::Threads
)

if(MSVC)
    target_compile_options(setbox_test PRIVATE /W4)
else()
    target_compile_options(setbox_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Main application executable
# -----------------------------------------------------------------------------

# Core sources (all platforms)
set(NEXRX_APP_SOURCES
    src/main.cpp
    src/FontRenderer.cpp
    src/AudioEngine.cpp
    src/WaterfallRenderer.cpp
)

if(NEXRX_REMOTE_TWIN)
    # Windows/macOS: use cross-platform socket abstraction and client transports
    list(APPEND NEXRX_APP_SOURCES
        src/net/Socket.cpp
        src/twin/TcpControlClient.cpp
        src/twin/UdpStreamClient.cpp
        src/twin/HostApp.cpp
    )
endif()

add_executable(nexrx_app ${NEXRX_APP_SOURCES})

target_include_directories(nexrx_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
    ${SDL2_INCLUDE_DIRS}
    ${TWIN_DIR}/src
)

# Platform-specific audio libraries for miniaudio
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(AUDIO_LIBS dl m)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    set(AUDIO_LIBS ${COREAUDIO_LIBRARY} ${AUDIOUNIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(AUDIO_LIBS winmm)
endif()

# Link libraries - all platforms use TCP/UDP to twin
target_link_libraries(nexrx_app PRIVATE
    setbox
    tinycbor
    ${SDL2_LIBRARIES}
    OpenGL::GL
    Threads::Threads
    ${AUDIO_LIBS}
)
target_compile_definitions(nexrx_app PRIVATE NEXRX_REMOTE_TWIN)

# Windows needs Winsock2
if(WIN32)
    target_link_libraries(nexrx_app PRIVATE ws2_32)
endif()

# Compiler-specific warning flags
if(MSVC)
    target_compile_options(nexrx_app PRIVATE /W4)
else()
    target_compile_options(nexrx_app PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
    )
endif()

# -----------------------------------------------------------------------------
# Copy config and lua files to build directory
# -----------------------------------------------------------------------------
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Create lua directory structure in build
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lua)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lua/ui)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lua/app)

# Copy lua files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/lua/
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/lua)
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "NexRx App Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Target system: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Lua: ${LUA_LIBS}")
message(STATUS "  SDL2: ${SDL2_LIBRARIES}")
message(STATUS "  OpenGL: ${OPENGL_LIBRARIES}")
if(NEXRX_WINDOWS)
    message(STATUS "  Windows build: ON (cross-platform transport)")
endif()
message(STATUS "")
