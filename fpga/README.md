# FPGA Design for NexRx Receiver (NexRig Project)

## Overview
This directory contains the SystemVerilog implementation for the FPGA portion of the NexRx receiver (part of the NexRig project), primarily handling:
- Numerically Controlled Oscillator (NCO) for Direct Digital Synthesis (DDS)
- High-speed SPI interface to STM32
- Real-time phase modulation control
- Timing and synchronization

## Directory Structure
- `rtl/` - SystemVerilog RTL source files
- `tb/` - Testbenches
- `constraints/` - Pin assignments and timing constraints
- `sim/` - Simulation scripts and waveforms
- `build/` - Build output and bitstreams

## Target Device
Lattice iCE40UP5K-SG48I (Ultra Plus FPGA)

## Key Modules
- NCO/DDS for phase generation
- SPI slave interface
- Clock management
- GPIO control for PIN diodes
