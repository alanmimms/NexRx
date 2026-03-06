# NexRx: System Overview
## Open-Source HF SDR Receiver

**Document Version:** 1.0
**Date:** December 2025
**License:** MIT License (hardware, firmware, and software)

---

## Table of Contents

1. [Introduction](#introduction)
2. [Key Innovations](#key-innovations)
3. [System Architecture](#system-architecture)
4. [Power Architecture](#power-architecture)
5. [Connectivity Architecture](#connectivity-architecture)
6. [Host PC Application](#host-pc-application)
7. [Operating Concept: SetBoxes](#operating-concept-setboxes)
8. [Target Users](#target-users)
9. [System Specifications](#system-specifications)

---

## Introduction

NexRx is an open-source software-defined radio (SDR) receiver that is
the receiver portion of the NexRig project—a full transceiver. NexRx is a
general coverage HF receiver aimed at the ham radio HF bands but it
covers 1-30 MHz without gaps. Unlike traditional commercial receivers,
NexRx combines modern computing capabilities with sophisticated RF
engineering to create a platform that is both powerful and accessible
to the amateur radio community.

The entire project—hardware schematics, PCB designs, firmware source
code, FPGA designs, and host application software—is released under
the MIT License. This permissive license encourages experimentation,
modification, and contribution from the global amateur radio
community.

### Design Philosophy

NexRx represents a fundamental rethinking of receiver architecture:

**Digital-First Engineering**: Where traditional radios use analog
circuits, NexRx leverages digital control and processing. The Host
PC, STM32H753 microcontroller, and Lattice FPGA handle tasks
traditionally performed by analog components for flexibility and
precision.

**Impedance Domain Optimization**: Rather than forcing all subsystems
to operate at the traditional 50Ω impedance, NexRx uses optimized
impedance domains for each function. The receiver digital attenuator
bank and preselector operate nominally at 200Ω for superior
selectivity and lower component stress.

**Software-Defined Configuration**: The SetBox paradigm replaces
traditional knobs and buttons with hierarchical configuration
inheritance. Complex operating scenarios become simple profile
switches, with all parameters managed intelligently by the software.

---

## Key Innovations

### Triple-QSD Receiver Architecture

NexRx's receiver uses three independent Quadrature Sampling Detectors
(QSDs) operating simultaneously to achieve superior harmonic rejection
without traditional image frequency problems.

**QSD0** operates at frequency offset *f-k*, providing the primary
receive signal path with excellent sensitivity.

**QSD1** operates at frequency offset *f+k*, enabling image rejection
through complex signal combining with QSD0. The complementary
frequency offsets allow software to separate desired signals from
images that would plague traditional superheterodyne architectures.

**QSD2** operates directly at frequency *f* with a specialized 33.33%
duty cycle clock (6× oversampling). This unusual duty cycle provides
greater than 40 dB rejection of third-harmonic responses, effectively
eliminating a major spurious response mechanism in direct-conversion
receivers.

The three QSD paths feed six channels (I and Q for each QSD) all
synchronized to a common clock into MAX9939 programmable gain
amplifiers and then into an AK5578 eight-channel audio codec sampling
at 96 kHz. The STM32 combines the three complex baseband signals in
software, creating a receiver with exceptional dynamic range and
spurious-free response.


### SetBox Configuration Paradigm

Traditional radios force operators to adjust many independent
parameters every time they change bands, modes, or operating
situations. A contest operator switching from 20-meter CW to 40-meter
SSB might need to change: frequency, mode, antenna selection, power
level, audio equalization, AGC settings, noise reduction, filter
bandwidth, and more.

NexRx's SetBox software configuration system uses hierarchical
inheritance. Create a "Contest-Base" SetBox defining common contest
settings—perhaps reduced power for battery operation and your contest
callsign. Create "Contest-20m" and "Contest-40m" SetBoxes inheriting
from Contest-Base but adding band-specific antenna selections and
frequency memories. Create "Contest-20m-CW" and "Contest-40m-SSB"
SetBoxes that inherit from their respective band configurations and
add mode-specific parameters.

When you switch from Contest-20m-CW to Contest-40m-SSB, the system
instantly reconfigures everything—antenna, power, frequency, mode,
audio processing, waterfall colors, even active keyboard shortcuts.
Settings common to all contest operations (inherited from
Contest-Base) remain consistent, while band and mode specifics adjust
automatically.

The SetBox hierarchy makes complex operating scenarios trivial to
manage while maintaining complete control for operators who want to
adjust individual parameters.

---

## System Architecture

## Connectivity Architecture

### Data Streams

**Receive I/Q Data** (STM32 → Host):
- Six channels (I and Q for QSD0, QSD1, QSD2)
- 96 kHz sample rate per channel (after decimation from 96 kHz AK5578)
- 24-bit samples (3 bytes per sample)
- Total data rate: 6 channels × 96,000 samples/sec × 3 bytes = ~1.7 MB/s (13.8 Mbps)
- Transport must deliver minimal latency

**Control/Status Messages** (Bidirectional):
- Relay switching commands (band, attenuator, T/R)
- PGA gain adjustments
- FPGA frequency/phase settings
- Temperature/voltage telemetry
- Calibration data
- TCP transport for reliability
- Low bandwidth (~100 kB/s peak)

Total bandwidth: ~15 Mbps well within 100 Mbps Ethernet capability
(Fast Ethernet), leaving substantial headroom. The Ethernet vs USB2
(480Mb/s) decision is still pending, but it's likely both will be
supported in some fashion.


## Host PC Application

### Native Application

The NexRx user interface is a **native desktop application**. The
framework to use for this is still TBD.

**Cross-Platform**: A single codebase produces native applications for:
- Windows (10/11)
- macOS (11.0+)
- Linux (Ubuntu, Fedora, Arch, etc.)

The application is packaged using a method that is TBD, creating
platform-specific installers (`.exe`, `.dmg`, `.AppImage`, `.deb`)
with appropriate signing and notarization.

### Application Architecture

**Main Process**
- Manages application lifecycle (startup, shutdown, updates)
- Creates and controls application window
- Handles Ethernet and/or USB socket connections (TCP/UDP, etc)
- Interfaces with native OS features (file system, audio devices)
- Manages SetBox storage (JSON files on disk)

**Renderer Process**
- TBD framework based user interface components
- DSP processing
- Waterfall display
- Real-time visualizations (S-meter, spectrum, constellation)


### DSP Responsibilities

The host application performs **computationally intensive DSP**
unsuitable for real-time embedded processing:

**Triple-QSD Combining**: The three complex baseband signals (from
QSD0, QSD1, QSD2) are combined with calibrated weights to maximize
signal and reject images/harmonics.

**Demodulation**: SSB, CW, AM, FM, and digital mode (PSK, FT8, RTTY)
demodulation algorithms.

**Filtering**: User-adjustable passband filters, notch filters, and
noise reduction filters applied in the frequency domain (FFT-based).

**AGC (Automatic Gain Control)**: Adaptive gain control with
configurable attack/release times to maintain consistent audio levels.

**Audio Processing**: De-emphasis and bass/treble controls for receive
audio.

**Visualization**: Real-time FFT for waterfall and spectrum displays,
constellation diagrams for digital modes, waveform display.

### User Interface Highlights

**Waterfall Display**: Primary tuning interface. Mouse wheel scrolling
tunes frequency. Click-and-drag selects signals. Zoom controls adjust
span.

**SetBox Selector**: Dropdown or tree view showing available
configurations. One-click switching between operating scenarios.

**Control Panel**: Consolidated access to frequency, mode, power,
antenna selection, audio controls, and advanced settings.

**Status Indicators**: S-meter, VSWR, temperature, power output,
voltage rails—all updated in real-time.

**Digital Mode Integration**: Built-in decoders for popular digital
modes with automatic frequency tracking and logging.

---

## Operating Concept: SetBoxes

### The Problem with Traditional Radios

A typical contest operator switching from 20-meter CW to 40-meter SSB
must manually adjust:
- Frequency (VFO)
- Mode (CW → SSB)
- Antenna selection (20m beam → 40m dipole)
- Filter bandwidth (CW narrow → SSB wide)
- AGC settings (CW fast → SSB slow)
- Waterfall span and color scheme

Traditional radios force operators to remember and execute this
sequence every time they change bands or modes. Memory channels help
with frequency but leave all other settings independent.

### The SetBox Solution

A **SetBox** is a named configuration containing values for every
adjustable parameter in the system. SetBoxes form **inheritance
hierarchies**, where child SetBoxes inherit parameter values from
parents and override only what changes.

**Example Hierarchy**:

```
Global-Defaults
    ├─ Contest-Base (inherits from Global-Defaults)
    │   ├─ Contest-20m (inherits from Contest-Base)
    │   │   ├─ Contest-20m-CW (inherits from Contest-20m)
    │   │   └─ Contest-20m-SSB (inherits from Contest-20m)
    │   └─ Contest-40m (inherits from Contest-Base)
    │       ├─ Contest-40m-CW (inherits from Contest-40m)
    │       └─ Contest-40m-SSB (inherits from Contest-40m)
    └─ Ragchew-Base (inherits from Global-Defaults)
        ├─ Ragchew-20m-SSB
        └─ Ragchew-40m-SSB
```

**Global-Defaults** might define:
- Default AGC: Medium
- Default antenna: Auto-select by band

**Contest-Base** inherits everything from Global-Defaults but overrides:
- Callsign: W1ABC/M (mobile operation)

**Contest-20m** inherits from Contest-Base and overrides:
- Frequency: 14.000 MHz starting point
- Antenna: Force 20m beam selection

**Contest-20m-CW** inherits from Contest-20m and overrides:
- Mode: CW
- Filter bandwidth: 500 Hz
- AGC: Fast
- Keyboard shortcuts: CW-specific

When the operator switches to Contest-20m-CW, the system applies:
- Callsign: W1ABC/M (from Contest-Base)
- Frequency: 14.000 MHz (from Contest-20m)
- Antenna: 20m beam (from Contest-20m)
- Mode: CW (from Contest-20m-CW)
- Filter: 500 Hz (from Contest-20m-CW)
- AGC: Fast (from Contest-20m-CW)

Switching to Contest-40m-SSB instantly reconfigures to:
- Callsign: W1ABC/M (from Contest-Base, unchanged)
- Frequency: 7.200 MHz (from Contest-40m)
- Antenna: 40m dipole (from Contest-40m)
- Mode: SSB (from Contest-40m-SSB)
- Filter: 2.4 kHz (from Contest-40m-SSB)
- AGC: Slow (from Contest-40m-SSB)

### Benefits

**Consistency**: Settings common to all contest operations (power,
callsign) are defined once in Contest-Base and automatically apply to
all child SetBoxes.

**Flexibility**: Any parameter can be overridden at any level of the
hierarchy. Want more power on 40m SSB for DX? Override in
Contest-40m-SSB without affecting other configurations.

**Simplicity**: Switching between complex operating scenarios becomes
a single SetBox selection. The system handles all parameter changes
atomically.

**Transparency**: The user interface shows exactly where each active
parameter value comes from in the inheritance chain. Adjusting a
parameter prompts: "Save to current SetBox?" or "Save as new child
SetBox?"

**Experimentation**: Trying new settings is safe. Create a new child
SetBox, experiment, and discard if unsuccessful. The parent SetBox
remains unchanged.

---

## Target Users

### Amateur Radio Operators

**Experienced Operators**: Hams who understand radio theory and want
maximum control over their equipment. NexRx provides direct access to
every parameter while simplifying complex operations through SetBoxes.

**Digital Mode Enthusiasts**: Built-in decoders and optimized
configurations for PSK31, FT8, RTTY, and other digital modes. The host
PC's computational power enables sophisticated decoding and automatic
frequency tracking.

**Contesters**: Rapid band/mode switching via SetBoxes. Multiple
receiver windows. Logging integration.

**Experimenters**: Open-source design invites modification. Add new
digital modes, implement advanced DSP algorithms, design custom UI
panels—all without hardware changes.

### Developers and Researchers

**RF Engineers**: Complete schematics and design documentation enable
detailed analysis and optimization. The modular architecture supports
subsystem replacement without redesigning the entire radio.

**Software Developers**: Electron/React UI is accessible to web
developers. Contribution doesn't require embedded systems expertise.
The API between hardware and software is well-documented and stable.

**Educators**: NexRx serves as a teaching platform for SDR concepts,
DSP algorithms, RF engineering, and software architecture. Students
can experiment with real hardware while understanding complete signal
flow.

### Open-Source Community

Anyone interested in advancing amateur radio technology. Contributions
range from documentation improvements to hardware revisions to
software enhancements. The MIT License ensures that improvements
benefit the entire community.

---

## System Specifications

### Frequency Coverage

**Receive**: 1.0 - 30 MHz continuous (general coverage)

### Receiver Performance

**Architecture**: Triple-QSD direct conversion with complementary harmonic rejection
**Preselection**: Variable LC tuning, 200Ω nominal impedance
**Sampling Rate**: 96 kHz (six channels: I/Q for each of three QSDs)
**ADC Resolution**: 24-bit (AK5578)
**Dynamic Range**: >100 dB (achievable with triple-QSD combining and gain ranging)
**MDS (Minimum Detectable Signal)**: Better than -130 dBm (estimated, 500 Hz BW, 10 dB SNR)

### Impedance Domains

**Receiver Preselector**: 200Ω nominal (optimized for selectivity and reduced component stress)
**Antenna Interface**: 50Ω (standard)

### Physical Interfaces

**Power**: USB-C connector, default USB 5V at 3A or even 1A.
**Data**: RJ45 Ethernet, 100 Mbps (Fast Ethernet) or USB2 480Mb/s (still pending).
**Antenna**: SO-239 or Type-N connector (TBD)

### Control and Monitoring

**Microcontroller**: STM32H753, 480 MHz Cortex-M7, 1MB flash, 1MB RAM, Zephyr RTOS
**FPGA**: Lattice iCE40UP5K-SG48I, 40 MHz TCXO reference, NCO and clock generation
**Voltage Monitoring**: USB VBUS input

---

## Conclusion

NexRx represents a modern approach to amateur radio receiver
design, combining sophisticated RF engineering with flexible software
control. The triple-QSD receiver architecture and SetBox configuration
system provide capabilities typically found only in commercial
equipment costing thousands of dollars.

The open-source nature of the project—hardware, firmware, and
software—encourages experimentation, learning, and community
contribution. Whether you're an experienced RF engineer, a software
developer, or an amateur radio operator seeking a powerful and
flexible receiver, NexRx offers a platform for exploration and
innovation.

The following documents provide detailed technical information:

- **RX-ARCHITECTURE.md**: Complete receiver design (preselector, QSDs, PGAs, ADC, AGC)
- **SYSTEM-INTEGRATION.md**: Power system, connectivity, protocols, firmware/FPGA design, calibration
- **CONSTRUCTION-TESTING.md**: Assembly procedures, testing, calibration, validation

Welcome to the NexRx project. We look forward to your contributions and innovations.

---

**Document Revision**: 1.0
**Last Updated**: January 2026
**Project Repository**: https://github.com/alanmimms/nexrx.git
**License**: MIT License - See LICENSE.md
