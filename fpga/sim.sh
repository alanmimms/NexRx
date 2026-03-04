#!/bin/bash
# Simple simulation script for NexRx FPGA using Icarus Verilog

set -e

# Collect all RTL files
RTL_FILES="rtl/nco-core.sv rtl/phase-gen-4.sv rtl/phase-gen-6.sv rtl/timer-64bit.sv rtl/register-bank.sv rtl/noise-gen.sv rtl/nexrx-top.sv"
TB_FILE="rtl/nexrx-top-tb.sv"

echo "Compiling with Icarus Verilog..."
iverilog -g2012 -DSIMULATION -o nexrx-sim $RTL_FILES $TB_FILE

echo "Running Simulation..."
vvp nexrx-sim

echo "Simulation Done."
