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
    src/app/AGCManager.cpp
    src/drivers/FPGAManager.cpp
    src/drivers/AK5578.cpp
    src/drivers/MAX9939.cpp
    src/drivers/NexBus.cpp
    src/drivers/DisplayManager.cpp
    src/transport/USBManager.cpp
    src/transport/QSDCapture.cpp
    src/transport/ControlHandler.cpp
    src/transport/USBCDCTransport.cpp
)

# Include directories
target_include_directories(app PRIVATE src)
