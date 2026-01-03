# NexRx Digital Twin Implementation Plan

**Project:** NexRx Triple-QSD SDR Receiver Digital Twin
**Location:** `/home/alan/ham/NexRx/twin/`
**Date:** January 2026

---

## Executive Summary

Build a Software-in-the-Loop (SiL) simulation environment combining:
1. **Xyce** (SPICE analog solver) for RF physics
2. **Zephyr native_sim** for bit-identical firmware execution
3. **C++ Orchestrator** to coordinate simulation steps
4. **Transport-agnostic IPC** abstracting the Host↔STM32 interface

This enables firmware development, DSP algorithm validation, and UI testing before hardware exists.

---

## Project Stages

### Stage 1: Foundation (CMake + Basic Orchestrator)
**Goal:** Minimal build system linking Xyce, basic simulation stepping

**Deliverables:**
- `twin/CMakeLists.txt` - CMake project setup
- `twin/src/main.cpp` - Entry point
- `twin/src/orchestrator/Orchestrator.hpp/.cpp` - Core simulation controller
- `twin/src/xyce/XyceWrapper.hpp/.cpp` - libxyce integration wrapper

**Key Files:**
- Xyce library: `/home/alan/ham/xyce/Xyce-7.10/build/src/.libs/libxyce.a`
- Trilinos deps: `/home/alan/ham/XyceLibs/Serial/lib/*.a`
- Xyce includes: `/home/alan/ham/xyce/Xyce-7.10/src/*/`

**Tasks:**
1. Create CMake build system with Xyce/Trilinos linkage
2. Implement XyceWrapper class exposing:
   - `initialize(netlistPath)`
   - `stepTo(time)` - advance simulation
   - `getNodeVoltage(nodeName)`
   - `setDeviceParam(device, param, value)`
3. Basic Orchestrator main loop skeleton

---

### Stage 2: NCO Engine (FPGA Surrogate)
**Goal:** Generate QSD switch timing signals

**Deliverables:**
- `twin/src/fpga/NcoEngine.hpp/.cpp` - NCO implementation
- `twin/src/fpga/QsdClockGen.hpp/.cpp` - Switch state generation

**Functionality:**
- 32-bit phase accumulator per NCO (3 total)
- Generate switch states at simulation time steps:
  - QSD0/1: 4-phase quadrature (0°, 90°, 180°, 270°)
  - QSD2: 6-phase sextature (0°, 60°, 120°, 180°, 240°, 300°)
- Output: `bool[4]` or `bool[6]` switch states per QSD

---

### Stage 3: RF Component Netlists
**Goal:** Xyce models for NexRx analog path

**Deliverables:**
- `twin/netlists/preselector.cir` - 800Ω LC bank with switched components
- `twin/netlists/transformer.cir` - Pentafilar transformer (K-factors)
- `twin/netlists/qsd.cir` - Triple-QSD with TS3A4751 switch models
- `twin/netlists/nexrx_rx.cir` - Full receiver path (top-level)
- `twin/netlists/stimulus.cir` - RF signal injection (subcircuit)

**Component Models:**
- TS3A4751 CMOS switches: time-varying resistors (Ron=0.9Ω, Roff=10MΩ)
- Inductors with Q and ESR from datasheet values
- Capacitor bank with parasitic modeling

---

### Stage 4: Transport Abstraction Layer
**Goal:** Abstract Host↔STM32 interface for USB/Ethernet agnosticism

**Deliverables:**
- `twin/proto/nexrx.proto` - Protobuf schema for control API
- `twin/src/transport/Transport.hpp` - Abstract interface
- `twin/src/transport/SharedMemTransport.hpp/.cpp` - POSIX shm for I/Q data
- `twin/src/transport/UnixSocketTransport.hpp/.cpp` - RPC channel
- `twin/src/transport/IQFrame.hpp` - 6-channel interleaved frame structure

**Protocol Design:**
```protobuf
message SetVfoFreq { uint64 freq_hz = 1; }
message SetPreselMask { uint32 inductor_mask = 1; uint32 cap_value = 2; }
message SetAttenDb { uint32 atten_db = 1; }
message IQSample { int32 i = 1; int32 q = 2; }  // 24-bit in int32
message IQFrame {
  repeated IQSample qsd0 = 1;  // I/Q pair
  repeated IQSample qsd1 = 2;
  repeated IQSample qsd2 = 3;
  uint64 timestamp_ns = 4;
}
```

---

### Stage 5: Zephyr native_sim Integration
**Goal:** Run actual STM32 firmware compiled for x86_64

**Deliverables:**
- `twin/zephyr/CMakeLists.txt` - Zephyr app for native_sim
- `twin/zephyr/prj.conf` - Project configuration
- `twin/zephyr/boards/native_sim.overlay` - Device tree overlay
- `twin/src/zephyr/FirmwareProcess.hpp/.cpp` - Process management

**Architecture:**
- Firmware runs as separate process
- Communicates with Orchestrator via Unix socket (RPC)
- Receives I/Q samples via shared memory
- Peripheral drivers redirect to Virtual Register Map

**Link to existing Zephyr:**
- `~/zephyr-projects/` as ZEPHYR_BASE
- Create app under `twin/zephyr/`

---

### Stage 6: Sample Generation Pipeline
**Goal:** Complete data flow from Xyce to "ADC samples"

**Deliverables:**
- `twin/src/sampler/AdcSampler.hpp/.cpp` - Sample Xyce nodes at 96kHz rate
- `twin/src/sampler/Decimator.hpp/.cpp` - Simulate anti-alias filtering

**Data Flow:**
1. Orchestrator runs Xyce in ~1ns steps (for 720MHz NCO resolution)
2. NCO Engine updates QSD switch states each step
3. Every 1/96000 seconds (~10.4μs), sample 6 nodes:
   - `qsd0_i`, `qsd0_q`, `qsd1_i`, `qsd1_q`, `qsd2_i`, `qsd2_q`
4. Pack into IQFrame, push to shared memory
5. Firmware process reads from shared memory

---

### Stage 7: Stimulus Sources
**Goal:** Inject test signals into simulation

**Deliverables:**
- `twin/src/stimulus/AntennaStimulus.hpp/.cpp` - Signal generator base
- `twin/src/stimulus/ToneGenerator.hpp/.cpp` - Single/multi-tone CW
- `twin/src/stimulus/NoiseGenerator.hpp/.cpp` - Thermal noise floor
- `twin/src/stimulus/RfCapturePlayer.hpp/.cpp` - Playback I/Q recordings

**Interface:**
```cpp
class AntennaStimulus {
public:
  virtual double getSample(double time_ns) = 0;  // Returns voltage
};
```

Xyce B-element (behavioral source) calls into C++ to get antenna voltage.

---

### Stage 8: Host Application Stub
**Goal:** Minimal host-side to exercise the twin

**Deliverables:**
- `twin/src/host/HostApp.hpp/.cpp` - Connect to transport, receive I/Q
- `twin/src/host/DspPipeline.hpp/.cpp` - Basic signal combining
- `twin/src/host/Visualizer.hpp/.cpp` - Terminal-based spectrum display (optional)

**Purpose:** Validate end-to-end data flow before full native app exists

---

## Directory Structure

```
twin/
├── CMakeLists.txt
├── Digital Twin Methodology and Architecture.md  (existing)
├── README.md
├── netlists/
│   ├── preselector.cir
│   ├── transformer.cir
│   ├── qsd.cir
│   ├── nexrx_rx.cir
│   └── stimulus.cir
├── proto/
│   └── nexrx.proto
├── src/
│   ├── main.cpp
│   ├── orchestrator/
│   │   ├── Orchestrator.hpp
│   │   └── Orchestrator.cpp
│   ├── xyce/
│   │   ├── XyceWrapper.hpp
│   │   └── XyceWrapper.cpp
│   ├── fpga/
│   │   ├── NcoEngine.hpp
│   │   ├── NcoEngine.cpp
│   │   ├── QsdClockGen.hpp
│   │   └── QsdClockGen.cpp
│   ├── transport/
│   │   ├── Transport.hpp
│   │   ├── IQFrame.hpp
│   │   ├── SharedMemTransport.hpp/.cpp
│   │   └── UnixSocketTransport.hpp/.cpp
│   ├── sampler/
│   │   ├── AdcSampler.hpp/.cpp
│   │   └── Decimator.hpp/.cpp
│   ├── stimulus/
│   │   ├── AntennaStimulus.hpp
│   │   ├── ToneGenerator.hpp/.cpp
│   │   ├── NoiseGenerator.hpp/.cpp
│   │   └── RfCapturePlayer.hpp/.cpp
│   ├── zephyr/
│   │   └── FirmwareProcess.hpp/.cpp
│   └── host/
│       ├── HostApp.hpp/.cpp
│       └── DspPipeline.hpp/.cpp
├── zephyr/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   ├── boards/
│   │   └── native_sim.overlay
│   └── src/
│       └── main.c
└── test/
    └── (unit tests)
```

---

## Key Design Decisions

### 1. Time Synchronization
- Xyce is the "time master" - simulation time is authoritative
- NCO Engine queries Xyce for current time to compute phase
- ADC sampling occurs at exact 1/96000s intervals in simulation time
- Deterministic: can pause/resume, single-step, replay

### 2. Latency Awareness
- RPC calls (Unix socket) model real-world latency
- Host app must treat hardware API as asynchronous
- Shared memory I/Q stream is "fire and forget" like USB isochronous

### 3. Separation of Concerns
- `XyceWrapper`: Only knows about SPICE simulation
- `NcoEngine`: Only knows about phase accumulation
- `Orchestrator`: Coordinates but doesn't implement physics
- `Transport`: Abstracts communication, swappable for real USB/Ethernet later

### 4. Code Reuse
- Protobuf schema shared between:
  - Orchestrator (C++ libprotobuf)
  - Zephyr firmware (nanopb)
  - Future host app (C++ or other)
- Same `IQFrame` structure used in simulation and real hardware

---

## Dependencies

**Build Requirements:**
- CMake 3.20+
- C++20 compiler (GCC 11+ or Clang 14+)
- Xyce 7.10 (built): `/home/alan/ham/xyce/Xyce-7.10/build/`
- Trilinos libs: `/home/alan/ham/XyceLibs/Serial/`
- Protobuf + nanopb
- Zephyr SDK (for native_sim)

**Runtime:**
- Linux (POSIX shared memory, Unix sockets)

---

## Implementation Order

| Stage | Description | Estimated Complexity |
|-------|-------------|---------------------|
| 1 | CMake + Xyce linkage + basic wrapper | Medium |
| 2 | NCO Engine | Low |
| 3 | RF Netlists (requires RF knowledge iteration) | High |
| 4 | Transport layer + protobuf | Medium |
| 5 | Zephyr native_sim setup | Medium-High |
| 6 | ADC sampling pipeline | Medium |
| 7 | Stimulus sources | Low-Medium |
| 8 | Host stub | Low |

**Recommended Start:** Stages 1-2 in parallel, then 4, then 3+5+6, finally 7+8.

---

## Design Decisions (User Confirmed)

1. **Simulation Speed**: Configurable via runtime flag
   - Default: Accuracy mode (full physics, slower than real-time)
   - Optional: Real-time mode (coarser stepping, approximations)

2. **Zephyr Firmware Strategy**: Start fresh
   - Build twin-specific firmware from scratch
   - Focus on correct interfaces first
   - Integrate/port real FW code later once interfaces are proven

3. **Implementation Start Point**: Stage 1 (CMake + Xyce)

## Open Questions

1. **Signal injection interface**: B-element callback or Xyce PWL source from file?

## Implementation Notes

### Real-Time Mode - Adaptive Stepping (TODO)
Stage 1 testing revealed Xyce runs ~250,000× slower than real-time with 1ns steps.
When implementing real-time mode, need to investigate:
- Xyce's internal adaptive time-stepping vs. fixed steps
- Coarser time resolution (10ns, 100ns) tradeoffs
- Possible use of Xyce's "fast" transient options
- Model simplification for speed (behavioral vs. physical models)
- Consider hybrid approach: accurate RF physics at key sample points only

---

## Success Criteria

Stage 1-2 complete when:
- `./twin` executable builds and links
- Can load a simple RC netlist and step simulation
- NCO outputs correct phase for given frequency

Full twin operational when:
- Inject 14.2 MHz tone at antenna input
- Tune NCO to 14.2 MHz
- See correct I/Q output in host stub
- Firmware process responds to frequency change RPC
