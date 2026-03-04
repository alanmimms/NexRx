# NexRx STM32H753 Firmware
# Using Zephyr RTOS with C++20

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NexRxFW)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

target_sources(app PRIVATE src/main.cpp)
