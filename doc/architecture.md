# NexRx: Technical Architecture
## Hardware, Software, and RF Design

### Hardware Architecture

The NexRx hardware platform centers around an STM32H753
microcontroller - a 480MHz Cortex-M7 processor with 1MB of both flash
memory and RAM. This isn't overkill; real-time RF control, web
serving, and DSP processing demand serious computational resources.

```mermaid
graph LR
    A[Antenna] --> B[T/R Switch]

    C --> D[Digital attenuators]
    D --> E[HPF and BPF array]
    F --> I[QSD0 (f-k)]
    G --> I[QSD1 (f+k)]
    H --> I[QSD2 (tbd)]
    
    I --> J[6-channel audio codec]
    J --> K[STM32H753]
    K --> L[FPGA NCO]
```


### Host PC Interface

The receiver connects to a host PC running the native control app
through one of two physical connection options:

**USB 2.0 High-Speed (480 Mbps)**: The STM32 operates as a USB
ethernet gadget, presenting a standard network interface to the host
without requiring driver installation. This works seamlessly across
Windows, macOS, and Linux systems. This could alternatively be
implemented using USB audio 2.0 with 6 channels and a serial stream
for the control path. The decision about how to do this is TBD.

**100Mb Ethernet**: Direct Ethernet connection provides higher
bandwidth and longer cable runs, particularly useful for remote
installations or multi-operator stations. Implementation planned for
future software releases.

Both interfaces carry identical data streams using the same protocol
layer. The system streams three independent stereo (I/Q) channels at
96 kS/s with 24-bit resolution from the three quadrature sampling
detectors to the host. The host PC handles the majority of DSP
processing - demodulation, filtering, digital mode decoding, and the
user interface - while the STM32 maintains real-time RF control and
basic signal conditioning.

The host can be a laptop, desktop computer, or single-board computer
like a Raspberry Pi. The requirement is one of the supported OSes on a
supported architecture (probably only PC and ARM64) capable of doing
the DSP required.

### Software Architecture

The software stack divides responsibilities between the embedded system and the native host application, with each handling what it does best.

```mermaid
graph TB
    subgraph Native app ["Native App (Lua + Raylib + C++)"]
        A[Advanced DSP - C++]
        B[Setbox Config Management - Lua]
        C[Unified Widget UI - Lua]
        D[Visualizations - C++/Raylib]
        E[Digital Mode Decoding - C++]
    end
    
    subgraph Comms ["Communication Layer"]
        F[Direct TCP/UDP Sockets]
        G[Binary I/Q Data]
        H[CBOR/JSON Control Messages]
    end
    
    subgraph Embedded ["STM32 Embedded (C++20/Zephyr)"]
        I[Real-time RF Control]
        J[Basic DSP]
        K[Control Plane Server]
        L[USB/Ethernet]
    end
    
    Native app --> Comms
    Comms --> Embedded
```

### Receiver Architecture

The receive path implements a sophisticated three-receiver
architecture using independent quadrature sampling detectors (QSDs).
Each QSD operates as a direct-conversion mixer, producing I and Q
baseband signals that capture both amplitude and phase information of
the RF signal. The three receivers operate simultaneously, each with
independent frequency tuning controlled by separate FPGA-generated
local oscillator signals.


**Signal Path Components:**

The **preselector** provides digitally-tuned input bandpass filtering
ahead of the attenuators. Controlled via FPGA GPIO pins set by the
STM32 firmware, it uses a bank of digitally switched binary weighted
capacitors to optimize front-end selectivity based on the operating
frequency. This reduces out-of-band interference and improves dynamic
range.

The **attenuator pad array** consists of multiple switched attenuator
stages providing 0-63 dB of attenuation in 3 dB steps. The STM32
calculates required attenuation based on signal strength measurements
from the I/Q data, implementing automatic gain control (AGC). The
attenuators prevent overload of the QSD mixers during strong signal
conditions.


**AGC Implementation:**

AGC operates as a hybrid hardware/software system optimized for both
fast protection and smooth user experience:

1. **STM32 Fast Loop**: Measures I/Q signal strength every sample
   period, switches attenuator pads within microseconds to prevent
   overload
2. **Host PC Smoothing**: Applies gain compensation in DSP so the
   operator perceives smooth signal level changes rather than abrupt
   attenuator switching
3. **Attack/Decay**: Fast attack on strong signals (prevent overload),
   slow decay to avoid pumping on fading signals
4. **Setbox Control**: AGC characteristics (attack time, decay time,
   hang time, target level) configurable via setbox inheritance

The three QSD outputs feed into a multi-channel audio codec that
digitizes the six audio-rate signals (three I/Q pairs) at 96 kS/s with
24-bit resolution. This baseband data flows to the STM32 for basic
conditioning before transmission to the host PC for advanced DSP
processing.


### FPGA Subsystem

The FPGA serves as the high-speed signal processing and clock
generation hub, and a wide fan-out GPIO expander for slowly changing
signals like the digital capactiro bank selectors. It also handles
tasks requiring precise timing and high-speed logic.

**Master Clocking:**

A 40 MHz temperature-compensated crystal oscillator (TCXO) provides
the master timebase.

From this 40 MHz reference, the FPGA synthesizes all system clocks:

```
40 MHz TCXO
    ↓
┌───────────────┐
│  FPGA PLL/DCM │
└───────────────┘
    ├──→ NCO 1 Clock (QSD 1 LO)
    ├──→ NCO 2 Clock (QSD 2 LO) 
    ├──→ NCO 3 Clock (QSD 3 LO)
    ├──→ TX Phase NCO Clock
    ├──→ Audio Codec Master Clock (24.576 MHz for 96kS/s)
    └──→ STM32 External Clock (precise timing for USB/Ethernet)
```

**Numerically Controlled Oscillators (NCOs):**

The FPGA implements independent NCOs:

1. **RX NCO 0**: Generates quadrature sampling signals for `f-k` QSD
1. **RX NCO 1**: Generates quadrature sampling signals for `f+k` QSD
1. **RX NCO 2**: Generates quadrature sampling signals for `6f` QSD


### STM32 Subsystem

The STM32H753 microcontroller orchestrates all real-time control and
serves as the bridge between RF hardware and host PC.

**Peripheral Usage:**

**I2C Buses:**
- **Audio Codec Configuration**: Sets up the 6-channel audio codec

**SPI Interfaces:**

- **FPGA Configuration**: Bitstream loading and reconfiguration,
  read/write of control and status registers.

- **High-speed peripherals**: Future expansion

**USB/Ethernet:**
- **Primary Interface**: USB 2.0 High-Speed Ethernet gadget (480 Mbps)
  and/or 100Mb CAT5 Ethernet.

**Real-time Control Tasks:**

The STM32 runs Zephyr RTOS managing multiple concurrent tasks:
- RF switching coordination (T/R relay, MESFET gate, attenuators)
- AGC fast loop (signal strength → attenuator control)
- Power sequencing (startup and shutdown coordination)
- I/Q data conditioning and formatting
- Command processing from host PC


### Power Sequencing

The STM32 manages power-up and power-down sequencing for all
subsystems. Proper sequencing prevents damage and ensures reliable
startup.

**Note**: Detailed power sequencing design is still in development.
The following outlines requirements and approach:

**STM32 Power:**
- Single 3.3V rail, no sequencing required for STM32 itself
- VBAT pin connects to 3.3V (no battery backup planned)
- STM32 boots on internal 64 MHz HSI clock

**Controlled Power Rails:**
- FPGA core voltage (typically 1.1V) before FPGA I/O voltage (3.3V)
- TBD there are more of these to document.

**Startup Sequence (Planned):**
1. STM32 3.3V powers on
2. STM32 boots on internal clock
3. STM32 enables FPGA core voltage
4. STM32 enables FPGA I/O voltage
5. STM32 configures FPGA from SD card
6. STM32 switches to FPGA external clock
8. System ready for receive

**Shutdown Sequence (Planned):**
1. Power down FPGA I/O voltage
2. Power down FPGA core voltage
3. STM32 3.3V remains until power removed


### Development Standards and Practices

The project maintains consistent coding standards across both embedded
and host application components. All code uses CamelCase naming conventions for
variables, functions, and methods, with SNAKE_CASE reserved only for
constants and preprocessor macros.

The embedded system targets C++20 running on Zephyr RTOS, taking
advantage of modern language features for safer and more expressive
code. The host application leverages a high-performance C++ DSP core
integrated with a Lua 5.4 scripting environment for UI and high-level
logic, utilizing Raylib for hardware-accelerated rendering.

**Performance Requirements**: The system maintains sub-100ms response
times for critical RF parameter changes, continuous I/Q streaming at
96 kS/s, and smooth real-time waterfall displays. These requirements
drive many of the architectural decisions, particularly the division
of responsibilities between embedded and native host components.

**Communication Protocol**: The system uses binary frames for high-throughput I/Q data and structured messages (CBOR or JSON) for control and status updates. This hybrid approach optimizes for both efficiency and developer productivity.

### Hardware Interface Abstractions

The embedded software provides clean C++20 abstractions for all
hardware interfaces. Relay and pHEMT FET switching, power amplifier
control, and RF parameter adjustment all use object-oriented
interfaces that hide hardware complexity from higher-level code.

These abstractions enable rapid development and testing while
maintaining the real-time performance requirements of RF operation.
The modular design also simplifies hardware variations and upgrades
without requiring extensive software changes.

### Testing and Validation Strategy

Real-time RF systems present unique testing challenges that require
both automated testing and careful measurement validation. The project
includes provisions for automated testing of DSP algorithms,
communication protocols, and user interface components.

Hardware validation requires RF test equipment for measuring transmit
signal quality, receive sensitivity, and harmonic suppression. The
integrated predistortion system provides some self-monitoring
capability, but external measurement remains essential for complete
validation.

The open-source nature of the project enables distributed testing
across different operating environments and use cases, helping
identify issues that might not appear in controlled laboratory
conditions.
