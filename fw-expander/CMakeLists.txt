cmake_minimum_required(VERSION 3.16)

# Toolchain settings
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# Skip compiler check for bare metal
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(fw-expander C ASM)

set(MCU_FLAGS "-mcpu=cortex-m0plus -mthumb -mfloat-abi=soft")
set(CMAKE_C_FLAGS "${MCU_FLAGS} -Wall -Wextra -O2 -g -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS "${MCU_FLAGS} -specs=nosys.specs -Wl,--gc-sections -T${CMAKE_CURRENT_SOURCE_DIR}/src/stm32c011f4.ld")

add_executable(fw-expander
    src/main.c
    src/startup.c
)

target_include_directories(fw-expander PRIVATE include)
target_compile_definitions(fw-expander PRIVATE STM32C011xx)

# Post-build: Create hex and bin files, show size
add_custom_command(TARGET fw-expander POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex fw-expander fw-expander.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary fw-expander fw-expander.bin
    COMMAND ${CMAKE_SIZE} fw-expander
)
