cmake_minimum_required(VERSION 3.25)
project(NexRxSuperbuild)

# Global language settings for the architecture
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NEXRX_TOOLS_DIR "${PROJECT_SOURCE_DIR}/tools" CACHE PATH "Path to tools directory")

set(BOARD nexrx CACHE STRING "Target Zephyr Board" FORCE)
set(SHIELD display_uc1609 CACHE STRING "Target Zephyr Display Shield" FORCE)

include(ExternalProject)

# Host Desktop App (Native AMD64, Raylib, Lua)
ExternalProject_Add(nexrxApp
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/app"
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/app-prefix"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/app/build"
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> -j
  INSTALL_COMMAND ""
)

# CPLD image (CMake-driven Yosys/nextpnr/Verilator)
ExternalProject_Add(nexrxCpld
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/CPLD"
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/cpld-prefix"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/CPLD/build"
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target bitstream
  INSTALL_COMMAND ""
)

# Digital Twin (Simulation & stimulus generation)
ExternalProject_Add(nexrxTwin
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/twin"
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/twin-prefix"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/twin/build"
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> -j
  INSTALL_COMMAND ""
)

# Locate Python Virtual Environment and West
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/west")
  set(westExecutable "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/west")
  set(venvBinDir "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/venv/bin/west")
  set(westExecutable "${CMAKE_CURRENT_SOURCE_DIR}/tools/venv/bin/west")
  set(venvBinDir "${CMAKE_CURRENT_SOURCE_DIR}/tools/venv/bin")
else()
  set(toolsDir "${CMAKE_CURRENT_SOURCE_DIR}/tools")
  set(venvBinDir "${toolsDir}/venv/bin")
  set(westExecutable "${venvBinDir}/west")

  add_custom_command(
    OUTPUT "${venvBinDir}/west"
    COMMAND ${CMAKE_COMMAND} -E echo "Creating Python venv in ${toolsDir}/venv..."
    COMMAND python3 -m venv "${toolsDir}/venv"
    COMMAND "${venvBinDir}/python" -m pip install -r "${toolsDir}/requirements.txt"
    DEPENDS "${toolsDir}/requirements.txt"
    COMMENT "Bootstrapping superset Python virtual environment"
  )
  add_custom_target(nexrxVenv DEPENDS "${venvBinDir}/west")
endif()

# Locate Zephyr Base
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zephyr/Kconfig.zephyr")
  set(zephyrBase "${CMAKE_CURRENT_SOURCE_DIR}/zephyr")
elseif(DEFINED ENV{ZEPHYR_BASE})
  set(zephyrBase "$ENV{ZEPHYR_BASE}")
else()
  set(zephyrBase "$ENV{HOME}/zephyr-projects/zephyr")
endif()

# Locate Zephyr SDK
if(DEFINED ENV{ZEPHYR_SDK_INSTALL_DIR})
  set(zephyrSdkDir "$ENV{ZEPHYR_SDK_INSTALL_DIR}")
elseif(EXISTS "$ENV{HOME}/zephyr-sdk-1.0.1")
  set(zephyrSdkDir "$ENV{HOME}/zephyr-sdk-1.0.1")
else()
  set(zephyrSdkDir "")
endif()

# Embedded Firmware (STM32H7 + Zephyr RTOS)
ExternalProject_Add(nexrxFw
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fw"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/fw/build"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ${CMAKE_COMMAND} -E env 
    "PATH=${venvBinDir}:$ENV{PATH}"
    "ZEPHYR_BASE=${zephyrBase}"
    "ZEPHYR_SDK_INSTALL_DIR=${zephyrSdkDir}"
    "${westExecutable}" build -b ${BOARD} --shield ${SHIELD}
    "${CMAKE_CURRENT_SOURCE_DIR}/fw" 
    -d "${CMAKE_CURRENT_BINARY_DIR}/fw/build"
  INSTALL_COMMAND ""
  BUILD_ALWAYS TRUE
)

if(TARGET nexrxVenv)
  add_dependencies(nexrxFw nexrxVenv)
endif()

# Top-level meta target
add_custom_target(all_nexrx ALL
  DEPENDS nexrxApp nexrxCpld nexrxTwin nexrxFw
  COMMAND ${CMAKE_COMMAND} -E create_symlink "fw/build/zephyr/zephyr.bin" "${CMAKE_CURRENT_BINARY_DIR}/zephyr.bin"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "fw/build/zephyr/zephyr.elf" "${CMAKE_CURRENT_BINARY_DIR}/zephyr.elf"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "CPLD/build/CPLD.bin" "${CMAKE_CURRENT_BINARY_DIR}/CPLD.bin"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "app/build/nexrx-app" "${CMAKE_CURRENT_BINARY_DIR}/nexrx-app"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "twin/build/nexrx-signalgen" "${CMAKE_CURRENT_BINARY_DIR}/nexrx-signalgen"
  COMMENT "Created convenience symlinks in ${CMAKE_CURRENT_BINARY_DIR} to build targets"
)
