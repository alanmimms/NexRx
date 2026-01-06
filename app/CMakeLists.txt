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

# Build type defaults to RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

# LuaJIT - high performance Lua implementation
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

# SDL2 - windowing and input
pkg_check_modules(SDL2 REQUIRED sdl2)

# OpenGL
find_package(OpenGL REQUIRED)

# sol2 is header-only, we'll fetch it
include(FetchContent)

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

# Threads
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# SetBox library - tag-based configuration engine
# -----------------------------------------------------------------------------
add_library(setbox STATIC
    src/setbox/SetBox.cpp
)

target_include_directories(setbox PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LUAJIT_INCLUDE_DIRS}
)

target_link_libraries(setbox PUBLIC
    ${LUAJIT_LIBRARIES}
    sol2::sol2
)

target_compile_options(setbox PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

# -----------------------------------------------------------------------------
# Test executable for SetBox engine
# -----------------------------------------------------------------------------
add_executable(setbox_test src/setbox_test.cpp)

target_link_libraries(setbox_test PRIVATE
    setbox
    Threads::Threads
)

target_compile_options(setbox_test PRIVATE
    -Wall -Wextra -Wpedantic
)

# -----------------------------------------------------------------------------
# Main application executable
# -----------------------------------------------------------------------------
add_executable(nexrx_app
    src/main.cpp
    src/FontRenderer.cpp
)

target_include_directories(nexrx_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(nexrx_app PRIVATE
    setbox
    ${SDL2_LIBRARIES}
    OpenGL::GL
    Threads::Threads
)

target_compile_options(nexrx_app PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

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
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  LuaJIT: ${LUAJIT_LIBRARIES}")
message(STATUS "  SDL2: ${SDL2_LIBRARIES}")
message(STATUS "  OpenGL: ${OPENGL_LIBRARIES}")
message(STATUS "")
