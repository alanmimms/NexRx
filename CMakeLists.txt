cmake_minimum_required(VERSION 3.20)
project(NexRx)

# Global language settings for the architecture
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NEXRX_TOOLS_DIR "${PROJECT_SOURCE_DIR}/tools" CACHE PATH "Path to tools directory")

set(BOARD nexrx CACHE STRING "Target Zephyr Board" FORCE)
set(SHIELD Uc1609 CACHE STRING "Target Zephyr Display Shield" FORCE)

add_subdirectory(CPLD)

add_subdirectory(app)
add_subdirectory(twin)
add_subdirectory(uitest)
add_subdirectory(test-app)
add_subdirectory(display-test)

add_custom_target(fw ALL
    COMMAND west build -b nexrx -d ${CMAKE_BINARY_DIR}/fw
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fw
    COMMENT "Building Zephyr firmware via West"
  )
