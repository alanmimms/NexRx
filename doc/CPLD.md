# NexRx CPLD Architecture (iCE40LP384-SG32)

This document describes the design, register map, and verification
flow for the NexRx CPLD SystemVerilog logic (`CPLD/rtl/top.sv`).

## Architecture Overview

Two identical iCE40LP384 CPLD ICs are deployed in the system—one
dedicated to **OSD0** and one to **OSD1**. Each CPLD acts as a
GNSS-disciplined, precision 8-phase octature or 4-phase quadrature
clock generator.

### Key Components

*   **Walking Ring Phase Generator**: A software-configurable one-hot
    counter that converts the synthesizer input clock (`osdClk`, pin
    20) into non-overlapping multiphase switching signals
    (`nPh[0..7]`).
    *   **8-Phase Octature Mode**: Active below 15 MHz ($8 \times
        \text{VFO}$ input clock up to 120 MHz).
    *   **4-Phase Quadrature Mode**: Active above 15 MHz ($4 \times
        \text{VFO}$ input clock up to 120 MHz).
*   **Waterfall Viewport Hysteresis Rule**: To eliminate visual
    discontinuities and mode thrashing in the waterfall display around
    15 MHz, software evaluates the active waterfall viewport
    boundaries:
    *   Switch to **4-phase mode** occurs **only when the entire
        waterfall viewport is strictly above 15 MHz**.
    *   Switch to **8-phase mode** occurs **only when the entire
        waterfall viewport is strictly below 15 MHz**.
    *   When the waterfall viewport straddles 15 MHz (e.g. 14.9 MHz to
        15.1 MHz), the current clock mode is preserved.
*   **GNSS TCXO Discipline Counter**: A monotonic 64-bit counter
    clocked directly by the 40 MHz master TCXO (`tcxo`, pin 29) and
    latched on the rising edge of the 1pps timing pulse from the GNSS
    chip (`gnssPPS`, pin 32). The latched 64-bit value is read via SPI
    by the STM32 to compute exact TCXO frequency offset and drift.
*   **SPI Slave Interface**: A 40-bit register-mapped SPI slave (8-bit
    address/command, 32-bit data payload).

## Pin Assignment Summary (`CPLD/rtl/pins.pcf`)

| Signal Name | CPLD Pin | Direction | Description |
| :--- | :--- | :--- | :--- |
| `osdClk` | Pin 20 | Input | Clock input from Si5351 synthesizer output. |
| `tcxo` | Pin 29 | Input | 40 MHz TCXO reference clock. |
| `gnssPPS` | Pin 32 | Input | 1 Hz precision pulse input directly from GNSS receiver. |
| `fpgaMOSI` | Pin 13 | Input | SPI Master-Out Slave-In from STM32. |
| `fpgaMISO` | Pin 12 | Output | SPI Master-In Slave-Out to STM32. |
| `fpgaSCK` | Pin 15 | Input | SPI Serial Clock from STM32. |
| `fpgaNSS` | Pin 14 | Input | SPI Active-Low Chip Select from STM32. |
| `nPh[0..7]` | Pins 1, 5, 8, 2, 18, 22, 23, 19 | Output | Non-overlapping 8-phase clock outputs to OSD switches. |

## Register Map (32-bit wide)

See `CPLD/rtl/regs.py` for register generators.

| Address | Mode | Name | Description |
| :---: | :---: | :---: | :--- |
| **0x00** | RW | `Control` | Bit 0: Mode Select (0 = 8-phase Octature, 1 = 4-phase Quadrature). Bit 1: Soft Reset. |
| **0x01** | RO | `PPSLatchHi` | High 32 bits of the 64-bit TCXO frequency counter latched on the last 1pps edge. |
| **0x02** | RO | `PPSLatchLo` | Low 32 bits of the 64-bit TCXO frequency counter latched on the last 1pps edge. |
| **0x7F** | RO | `Sig` | Hardware signature validation. Always reads `0x4E785278` ("NxRx"). |

## Simulation & Verification

The `CPLD/` directory contains complete SystemVerilog testbenches
(`tb.cpp`) and verification tooling:
*   **Simulation**: `verilator`, `gtkwave`, or `surfer`.
*   **Synthesis & PnR**: `yosys`, `nextpnr-ice40`, and `icepack`.

