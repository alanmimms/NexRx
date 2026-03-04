# NexRx FPGA Architecture (iCE40UP5K)

This document describes the design, register map, and verification flow for the
NexRx FPGA SystemVerilog logic.

## Architecture Overview

The FPGA acts as the precision clock generator for the three QSD (Quadrature 
Sampling Detector) stages and provides a high-precision timebase for the MCU.

### Key Components

*   **PLL**: Multiplies the 40 MHz TCXO to a **180 MHz** internal Master Clock.
*   **NCO Core**: 32-bit phase accumulators with **Shadow Registers** for 
    glitchless simultaneous frequency updates.
*   **Walking Rings**: One-hot counters that convert NCO pulses into 
    non-overlapping clock phases (4-phase for QSD0/1, 6-phase for QSD2).
*   **Reference Timer**: A monotonic 64-bit counter clocked directly by the 
    40 MHz TCXO.
*   **SPI Slave**: A 40-bit register-mapped interface (8-bit CMD, 32-bit DATA).

## Register Map (32-bit wide)

| Address | Mode | Name | Description |
| :--- | :--- | :--- | :--- |
| **0x00** | R/W | `REG_SCRATCH` | Scratchpad for SPI validation. |
| **0x01** | WO | `REG_COMMIT` | Write `1` to apply shadow frequency updates. |
| **0x04** | RO | `REG_TIME_L` | 64-bit Timer (Low 32). Latching: snap High bits. |
| **0x05** | RO | `REG_TIME_H` | 64-bit Timer (High 32). Returns latched value. |
| **0x10** | R/W | `REG_ISG_INC` | Signal Generator Phase Increment (Shadowed). |
| **0x20** | R/W | `REG_QSD0_INC` | QSD0 (f-k) Phase Increment (Shadowed). |
| **0x30** | R/W | `REG_QSD1_INC` | QSD1 (f+k) Phase Increment (Shadowed). |
| **0x40** | R/W | `REG_QSD2_INC` | QSD2 (f) Phase Increment (Shadowed). |

## Phasing & Duty Cycles

*   **QSD0/1 (4-phase)**: 25% duty cycle quadrature clocks. MCU writes 
    `4 * (f ± k)`.
*   **QSD2 (6-phase)**: 33.33% duty cycle sextature clocks for 3rd harmonic 
    rejection. MCU writes `6 * f`.

## Simulation & Verification

The design includes a SystemVerilog testbench (`rtl/nexrx-top-tb.sv`) that 
models the MCU SPI master.

### Prerequisites

*   **Simulation**: `iverilog` (Icarus Verilog), `vvp`, and `gtkwave`.
*   **Synthesis**: `yosys`, `nextpnr-ice40`, and `icepack`.

### Running Simulation

1.  Navigate to the `fpga/` directory.
2.  Run the simulation script:
    ```bash
    chmod +x sim.sh
    ./sim.sh
    ```
3.  View the results in GTKWave:
    ```bash
    gtkwave nexrx-fpga.vcd
    ```

## Build Instructions

The FPGA bitstream is built using the **Yosys/nextpnr-ice40** toolchain.

1.  **Synthesis**: `yosys -p "synth_ice40 -top nexrx_top -json nexrx.json" rtl/*.sv`
2.  **Place & Route**: `nextpnr-ice40 --up5k --package sg48 --json nexrx.json --asc nexrx.asc`
3.  **Bitstream**: `icepack nexrx.asc nexrx.bin`
