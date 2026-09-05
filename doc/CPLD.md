# NexRx CPLD Architecture (ICE40LP384-SG32)

This document describes the design, register map, and verification flow for the
NexRx CPLD SystemVerilog logic.

## Architecture Overview

The CPLD acts as a GNSS-disciplined, precision octature or quadrature
clock generator for the an Octature Synchronous Detector. Two
identical instances of this are used - one for each of the two OSDs.

### Key Components

*   **Walking Rings**: One-hot counters that convert synthesizer input
    clock pulses into non-overlapping clock phases (4-phase or
    8-phase).
*   **Frequency Counter**: A monotonic 64-bit counter clocked directly
    by the 40 MHz TCXO and latched by the 1Hz precision signal from
    the GNSS chip. This is used to discipline the TCXO by compensating
    for its inaccuracy in (STM32) software.
*   **SPI Slave**: A 40-bit register-mapped interface (8-bit CMD,
    32-bit DATA).

## Register Map (32-bit wide)

See `CPLD/rtl/regs.py` for more information. This is used to generate
`regs.sv`, `regs.h`, and `regs.md` documentation.

| Address | Mode | Name | Description |
| :---: | :--:- | :---: | :--- |
| **0x00** | RW | `Control` | Control and status flags. |
| **0x01** | RO | `PPSLatchHi` | High 32-bits of 64-bit TCXO frequency counter latched by each 1pps rising edge. |
| **0x02** | RO | `PPSLatchLo` | Low 32-bits of 64-bit TCXO frequency counter latched by each 1pps rising edge. |
| **0x7F** | RO | `Sig` | For validation of CPLD and SPI interface. Always reads 0x4E785278 ('NxRx'). |

## Simulation & Verification

The design includes a SystemVerilog testbench that models the MCU SPI
master.

### Prerequisites

*   **Simulation**: `verilator` and `gtkwave` or (better) `surfer`.
*   **Synthesis**: `yosys`, `nextpnr-ice40`, and `icepack`.

## Sim Instructions

XXX TBD

## Build Instructions

XXX TBD
