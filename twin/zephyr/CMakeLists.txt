# SPDX-License-Identifier: MIT
# NexRx Digital Twin - Zephyr Firmware for native_sim
#
# This builds the STM32 firmware to run on Linux x86_64 for
# software-in-the-loop testing with the Xyce RF simulation.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nexrx_twin_fw)

# C++20 for firmware
target_compile_features(app PRIVATE cxx_std_20)

# Main application sources
target_sources(app PRIVATE
    src/main.cpp
    src/twin_transport.cpp
    src/virtual_nco.cpp
    src/virtual_adc.cpp
)

# Include paths
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src  # Access twin headers (IQFrame, etc.)
)
