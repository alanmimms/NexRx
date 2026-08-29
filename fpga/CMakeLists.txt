cmake_minimum_required(VERSION 3.20)
project(NexRx_FPGA)

# Enable native resolution of VERILATOR_ROOT to suppress CMP0144 warning
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

find_package(Python3 REQUIRED)

# ------------------------------------------------------------------------------
# Register Generation (Python -> SV, C++, Markdown)
# ------------------------------------------------------------------------------
set(REG_GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/rtl/regs.py")
set(REG_TOOL_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/rtl/regTool.py")
set(REG_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/rtl/gen")

set(GENERATED_REGS
  "${REG_GEN_DIR}/regs.sv"
  "${REG_GEN_DIR}/regs.h"
  "${REG_GEN_DIR}/regs.md"
)

# Ensure the output directory exists before Python tries to write to it
file(MAKE_DIRECTORY ${REG_GEN_DIR})

# CONFIGURE-TIME EXECUTION:
# Because Verilator parses SystemVerilog during CMake configure time,
# we must physically generate the files right now, before verilate() is called.
message(STATUS "Executing regs.py to generate registers...")
execute_process(
  COMMAND ${Python3_EXECUTABLE} ${REG_GEN_SCRIPT} ${REG_GEN_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  RESULT_VARIABLE REG_RESULT
)

if(NOT REG_RESULT EQUAL 0)
  message(FATAL_ERROR "Register generation failed! Check regs.py.")
endif()

# Tell CMake to monitor the Python files. If you edit regs.py or regTool.py,
# Ninja/Make will automatically trigger a full CMake re-configure to rebuild regs.sv.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS 
  ${REG_GEN_SCRIPT} 
  ${REG_TOOL_SCRIPT}
)

# Create a target so Yosys and the C++ Testbench can still depend on the files at build time
add_custom_target(GenerateRegisters DEPENDS ${GENERATED_REGS})

# ------------------------------------------------------------------------------
# RTL Sources Definition
# ------------------------------------------------------------------------------
set(RTL_SOURCES
  "${REG_GEN_DIR}/regs.sv"
  "${CMAKE_CURRENT_SOURCE_DIR}/rtl/PhaseGen.sv"
  "${CMAKE_CURRENT_SOURCE_DIR}/rtl/RegAccess.sv"
  "${CMAKE_CURRENT_SOURCE_DIR}/rtl/timer64.sv"
  "${CMAKE_CURRENT_SOURCE_DIR}/rtl/top.sv"
)

# ------------------------------------------------------------------------------
# Yosys + NextPNR Synthesis (Target: bitstream)
# ------------------------------------------------------------------------------
set(PCF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rtl/pins.pcf")
set(SYNTH_JSON "${CMAKE_CURRENT_BINARY_DIR}/top.json")
set(SYNTH_ASC "${CMAKE_CURRENT_BINARY_DIR}/top.asc")
set(SYNTH_BIN "${CMAKE_CURRENT_BINARY_DIR}/top.bin")

add_custom_command(
  OUTPUT ${SYNTH_JSON}
  COMMAND yosys -p "read_verilog -sv ${RTL_SOURCES}; synth_ice40 -top top -json ${SYNTH_JSON}"
  DEPENDS ${RTL_SOURCES} # Natively waits for regs.sv because it is in the binary dir
  COMMENT "Synthesizing RTL with Yosys"
)

add_custom_command(
  OUTPUT ${SYNTH_ASC}
  COMMAND nextpnr-ice40 --lp384 --package sg32 --json ${SYNTH_JSON} --pcf ${PCF_FILE} --asc ${SYNTH_ASC}
  DEPENDS ${SYNTH_JSON} ${PCF_FILE}
  COMMENT "Placing and routing with nextpnr-ice40"
)

add_custom_command(
  OUTPUT ${SYNTH_BIN}
  COMMAND icepack ${SYNTH_ASC} ${SYNTH_BIN}
  DEPENDS ${SYNTH_ASC}
  COMMENT "Generating FPGA bitstream"
)

add_custom_target(bitstream ALL DEPENDS ${SYNTH_BIN})

# ------------------------------------------------------------------------------
# Verilator CPLD Simulation (Target: sim)
# ------------------------------------------------------------------------------
find_package(verilator HINTS $ENV{VERILATOR_ROOT})

if(verilator_FOUND)
  set(SIM_CPP "${CMAKE_CURRENT_SOURCE_DIR}/rtl/tb.cpp")
  add_executable(sim ${SIM_CPP})
  
  # Expose the generated headers in the build directory to the C++ testbench
  target_include_directories(sim PRIVATE ${REG_GEN_DIR})
  
  # Force the C++ compilation to wait for the header to physically exist
  set_property(SOURCE ${SIM_CPP} APPEND PROPERTY OBJECT_DEPENDS "${REG_GEN_DIR}/regs.h")

  # Use the native DEPENDS argument built into the verilate() macro
  verilate(sim
    SOURCES ${RTL_SOURCES}
    TOP_MODULE top
    PREFIX Vtop
    TRACE_VCD
    OPTS "-sv -Wall"
    DEPENDS ${GENERATED_REGS}
  )

else()
  message(WARNING "Verilator not found. Simulation target 'sim' will not be generated.")
endif()
