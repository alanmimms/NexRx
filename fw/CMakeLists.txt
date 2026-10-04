# NexRx STM32H743VIT6 Firmware
# Using Zephyr RTOS with C++20

cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NexRxFW)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
target_sources(app PRIVATE 
    src/main.cpp
    src/AGCManager.cpp
    src/Si5351Driver.cpp
    src/CPLDDriver.cpp
    src/TLV320ADC5140.cpp
    src/DisplayManager.cpp
    src/FrontEndDriver.cpp
    src/USBManager.cpp
    src/OSDCapture.cpp
    src/ControlHandler.cpp
    src/USBCDCTransport.cpp
)

# Include directories
target_include_directories(app PRIVATE src)

