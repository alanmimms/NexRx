cmake_minimum_required(VERSION 3.20)
project(NexRx)

# Global language settings for the architecture
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(CPLD)

add_subdirectory(app)
add_subdirectory(twin)

add_custom_target(fw ALL
    COMMAND west build -d build 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fw
    COMMENT "Building Zephyr firmware via West"
  )
