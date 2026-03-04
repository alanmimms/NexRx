# NexRx STM32H753 Firmware
# Using Zephyr RTOS with C++20

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NexRxFW)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
target_sources(app PRIVATE 
    src/main.cpp
    src/app/agc-manager.cpp
    src/drivers/fpga-manager.cpp
    src/drivers/ak5578.cpp
    src/drivers/max9939.cpp
    src/drivers/nexbus.cpp
    src/drivers/display-manager.cpp
    src/transport/usb-manager.cpp
    src/transport/qsd-capture.cpp
    src/transport/control-handler.cpp
    src/transport/usb-cdc-transport.cpp
)

# Include directories
target_include_directories(app PRIVATE src)
