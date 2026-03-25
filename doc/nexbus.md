# NexBus Protocol & Distributed I/O Specification

## Project Overview

The **NexBus** is a minimalist, high-reliability, single-wire
distributed control system designed for the **NexRig SDR**. It
replaces complex board-level routing and expensive I2C expanders with
a fleet of low-cost ($0.25) **STM32C011F** microcontrollers. Each
"Target" node provides 4-bit complementary GPO pairs (8 pins total) to
drive pHEMT RF switches and other analog peripherals with high timing
precision and low EMI.

---

## Hardware: The STM32C011F Target

The **STM32C011F** (UFQFN20 package) was selected for its ultra-low
cost, internal 48MHz oscillator, and robust "System Memory"
bootloader.

### Pin Mapping

* **PA11 / PA12 (NexBus):** Single-wire multidrop signal. Note: Per
  AN2606, USART1 is remapped to these pins on the UFQFN20 package.

* **PA0 – PA3 (True GPO):** Direct 4-bit output to pHEMT/Relay
  drivers.

* **PA4 – PA7 (Complement GPO):** Locally inverted versions of
  PA0–PA3.

* **PB7, PC14, PC15 (ID Pins):** Trinary encoding (High, Low,
  Floating).

* **Power:** 3.3VA/VCC with local 100nF decoupling. No `nRESET` or
  `BOOT0` lines are routed.

---

## Communication Protocols

The NexBus operates in two distinct modes over the same physical wire.

### Mode A: Runtime Pulse-Width Protocol (1 Mb/s effective)

A self-clocking, ratio-based protocol that is immune to oscillator
drift between the Host (STM32H7) and Targets.

* **Carrier Frequency:** 4 MHz (250ns base unit).

* **Symbol Encoding (High/Period Ratio):**

* **S0 (00):** 25% High (250ns High / 750ns Low)

* **S1 (01):** 50% High (500ns High / 500ns Low)

* **S2 (10):** 75% High (750ns High / 250ns Low)

* **S3 (11):** 100% High (Requires a 250ns Low spacer to maintain
  clocking)


* **The Frame:**

	[Target 0 (4 bits)] ... [Target N (4 bits)] + [Mode Nybble (4 bits)] + [SYNC]


* **SYNC Symbol:** A logic High pulse > 1.5 µs.

### Mode B: Firmware Update Protocol (115,200 Baud)

Standard STM32 Bootloader protocol (UART 8E1) used for initial
flashing and field updates.

* **Entry:** Triggered by a "Blind Parallel" broadcast or a specific
  "Jump to Bootloader" Mode Nybble command.

* **Half-Duplex:** Host and Targets share the same line. Targets use
  Open-Drain mode for responses to avoid bus contention during
  colliding ACKs.

---

## Telemetry Extension

To support sensor data, NexBus utilizes **Slotted Response Windows**.
1. **Master Request:** Master sends a standard frame with a specific
   Mode Nybble (0xC-0xE).
2. **Turnaround:** Master floats the bus for $500\mu s$.
3. **Target Response:** Targets respond sequentially based on ID.
4. **Encoding:** Targets use the same Ratio-Based Symbols
   (25%/50%/75%) to maintain protocol symmetry and simplify Host
   decoding.


## Identity & Addressing

Each target identifies itself at Power-On Reset (PoR) by probing three
pins (**PB7, PC14, PC15**).

### Trinary Detection Logic

The target firmware briefly enables internal Pull-Up and Pull-Down
resistors:

1. **Read with Pull-Down:** If Pin is High, State = **2 (High)**.

2. **Read with Pull-Up:** If Pin is Low, State = **0 (Low)**.

3. **Otherwise:** State = **1 (Floating)**.

**Total Addresses:** $3^3 = 27$ unique IDs.

---

## Recovery & Reliability

Because `nRESET` is not routed, the system uses "Software-Defined
Hardness":

1. **The Super-Break:** A logic Low held for **> 100 µs**. This is
   longer than any possible UART frame or Pulse symbol. It forces the
   Target state machine to reset to "Normal Mode" without altering
   current GPO states.

2. **Independent Watchdog (IWDG):** Targets must see a valid `SYNC`
   every 20ms. If the bus goes silent or hangs, the IWDG forces a
   local PoR.

3. **Pulse Width Filtering:** Functional targets ignore signals with
pulse widths > 5 µs, effectively "sleeping" through 115,200 baud UART
traffic intended for other nodes.

4. **Brown-Out Reset (BOR):** Set to Level 3 (~2.5V) via Option Bytes
   to prevent "glitch" execution during PA power sags.

---

## Update Workflow (Factory & Field)

### Initial Factory Flash

1. Blank chips PoR and automatically enter **System Bootloader**.

2. Host blasts `0x7F` (Auto-baud) followed by the binary image at
   115,200 baud.

3. All chips receive the same data in parallel. ACKs collide but are
   electrically compatible (Wired-AND).

### Targeted Field Update

1. Host sends a standard Pulse Frame with **Mode Nybble = [Target
   ID]**.

2. Only the matching Target executes `jump_to_bootloader()`.

3. Other targets see the long UART pulses, trigger their "Ignore
   Filter," and remain in the `Wait-for-Idle` state.

4. Host flashes the specific target and issues a **Super-Break** to
   return all units to 1Mb/s operation.

---

## Electrical Best Practices

* **Slew Rate:** Targets must be configured for **Low Speed** GPIO
  drive (`OSPEEDR`) to minimize RF interference.

* **Phased Switching:** The firmware staggers the "True" and
  "Complement" bit updates by 1–2 clock cycles (21–42ns) to soften the
  $di/dt$ spike.

* **Bus Damping:** A 33Ω series resistor at the STM32H7 source is
  recommended to prevent ringing across the 150mm multi-drop trace.
