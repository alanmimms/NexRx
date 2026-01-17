# NexRx Codebase Inventory

This document provides a complete inventory of the NexRx repository, explaining each file's purpose, which build artifact it belongs to, and key functions it provides.

**Project:** NexRx Triple-QSD SDR Receiver
**Generated:** 2026-01-17

---

## Table of Contents
1. [Root Level Files](#root-level-files)
2. [app/ - Host GUI Application](#app---host-gui-application)
3. [twin/ - Digital Twin Environment](#twin---digital-twin-environment)
4. [fw/ - STM32 Firmware (OBSOLETE)](#fw---stm32-firmware-obsolete)
5. [mock-server/ - Node.js Mock Server (OBSOLETE)](#mock-server---nodejs-mock-server-obsolete)
6. [hw/ - KiCad Hardware Design](#hw---kicad-hardware-design)
7. [Build Artifacts Summary](#build-artifacts-summary)
8. [External Library Dependencies](#external-library-dependencies)
9. [Recommendations](#recommendations)

---

## Root Level Files

| File | Purpose |
|------|---------|
| `.gitignore` | Git ignore patterns for build artifacts, editor files |
| `.CLAUDE` | Claude Code project configuration |

---

## app/ - Host GUI Application

**Build System:** CMake
**Target:** Host (Linux/Windows/macOS)
**Artifact:** `nexrx_app` executable
**Purpose:** Native GUI application for the NexRx receiver with SDL2/OpenGL rendering and Lua scripting

### Build Configuration

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Main build configuration. Builds `nexrx_app` and `setbox_test`. Links SDL2, OpenGL, Lua 5.4, sol2, tinycbor, miniaudio |
| `vcpkg.json` | vcpkg manifest for Windows/macOS dependencies |
| `.gitignore` | Ignores build/ directory |

### Code Generation

| File | Purpose |
|------|---------|
| `codegen/schema/rx_config.lua` | **Schema definition** for RxConfig - defines all receiver properties (volume, mode, filter settings, etc.) with types, ranges, and UI hints |
| `codegen/generator/generate_config.lua` | **Code generator** - reads schema, generates `RxConfig.hpp` and `RxConfig.cpp` with type-safe accessors, Lua bindings, change notifications |

### Configuration Files

| File | Purpose |
|------|---------|
| `config/base/defaults.lua` | SetBox default configuration values |
| `config/modes.lua` | **Mode presets** - defines filter/AGC settings for CW, SSB, AM, contest modes using SetBox tag-based rules |

### Lua UI System

| File | Purpose |
|------|---------|
| `lua/main.lua` | **Main UI script** - implements the entire GUI: VFO display, mode buttons, filter controls, S-meter, waterfall, volume controls. Calls C++ via bound functions |
| `lua/ui/widgets.lua` | Widget library - buttons, sliders, checkboxes, labels, panels |
| `lua/ui/layout.lua` | Layout engine - docking, padding, horizontal/vertical arrangement |
| `lua/ui/theme.lua` | Color theme definitions |
| `lua/ui/state.lua` | UI state management (hover, focus, active states) |

### External Libraries (Header-Only)

| File | Purpose |
|------|---------|
| `extern/miniaudio/miniaudio.h` | Single-header audio library for cross-platform audio I/O |
| `extern/stb/stb_truetype.h` | Single-header TrueType font rasterizer |

### Source Code - Main Application

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/main.cpp` | `main()`, `App` class, `processIQFrame()`, `computeSpectrum()` | **Main application** - ~1800 lines. SDL2 window, Lua integration, IQ processing with LMS image rejection, FFT spectrum, audio output. Contains the complete signal chain from UDP IQ frames to audio output |
| `src/AudioEngine.cpp/hpp` | `start()`, `stop()`, `setVolume()`, `startRecording()` | Audio output via miniaudio, WAV recording, test tone generation |
| `src/FontRenderer.cpp/hpp` | `drawText()`, `measureText()` | OpenGL text rendering using stb_truetype |
| `src/WaterfallRenderer.cpp/hpp` | `addRow()`, `render()`, `renderSpectrum()`, `setColormap()` | OpenGL waterfall display with multiple colormaps (viridis, plasma, etc.) |
| `src/Demodulator.hpp` | `process()`, `setMode()` | USB/LSB/AM/CW demodulation (header-only, ~100 lines) |

### Source Code - DSP

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/dsp/BasebandFilter.cpp/hpp` | `process()`, `setBandpassCenter()`, `setNotchCenter()`, `designBandpass()` | **FIR baseband filter** - complex bandpass and notch filters with windowed-sinc/Kaiser design, crossfade for glitch-free parameter changes |

### Source Code - Configuration System

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/setbox/SetBox.cpp/hpp` | `loadFile()`, `addTag()`, `removeTag()`, `resolve()`, `onPropertyChange()` | **Tag-based configuration engine** - resolves properties based on active tags (e.g., "cw" + "contest" + "20m") with rule priority |
| `src/setbox/Rule.hpp` | `Rule` struct | Data structure for SetBox rules (tags, properties, priority) |
| `src/setbox/TagSet.hpp` | `TagSet` class | Efficient tag set with hash-based matching |
| `src/config/ConfigBase.hpp` | `ConfigBase` template | Base class for generated config with change notifications |
| `src/config/PropertyMetadata.hpp` | `PropertyMetadata` struct | UI metadata (widget type, range, group) |
| `src/config/RemotePropertyHandler.hpp` | Interface | Abstract interface for remote property sync |
| `src/config/TwinRemoteHandler.hpp` | `TwinRemoteHandler` | Sends property changes to twin via TCP |

### Source Code - Networking

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/net/Socket.cpp/hpp` | `Socket` class | Cross-platform socket abstraction (Windows/POSIX) |
| `src/twin/TcpControlClient.cpp/hpp` | `connect()`, `sendCommand()`, `setVfo()` | TCP client for twin control commands |
| `src/twin/UdpStreamClient.cpp/hpp` | `start()`, `getFrame()` | UDP client for receiving IQ stream frames with CBOR decoding |
| `src/twin/HostApp.cpp/hpp` | `HostApp` class | Combines TCP control + UDP stream into single twin interface |

### Source Code - Buffers

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/buffer/RateAdaptiveBuffer.hpp` | `write()`, `read()`, rate adaptation | Lock-free ring buffer with adaptive rate matching for audio pipeline |

### Test Executable

| File | Purpose |
|------|---------|
| `src/setbox_test.cpp` | **SetBox unit test** - validates tag resolution, rule priority, property changes |

### Documentation

| File | Purpose |
|------|---------|
| `README.md` | App documentation (may be incomplete) |

---

## twin/ - Digital Twin Environment

**Build System:** CMake
**Target:** Host (Linux)
**Artifacts:** `twin`, `twin_xyce`, `stimulus_test`, `host_test`
**Purpose:** Software-in-the-loop simulation environment using Xyce SPICE or fast functional models

### Build Configuration

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Complex build with Xyce/Trilinos integration. Builds multiple libraries (twin_core, twin_transport, twin_stimulus, twin_sampler, twin_host, twin_firmware) and executables |
| `.gitignore` | Ignores build/ |

### Main Executables

| File | Artifact | Purpose |
|------|----------|---------|
| `src/signalgen.cpp` | `twin` | **Primary digital twin** - fast functional model with TCP/UDP streaming to app. Runs at real-time, generates synthetic IQ frames |
| `src/main.cpp` | `twin_xyce` | Full Xyce SPICE simulation (slow but accurate physics) |
| `src/stimulus_test.cpp` | `stimulus_test` | Tests stimulus generators (tones, noise, SSB) |
| `src/host_test.cpp` | `host_test` | Tests host DSP pipeline without Xyce |

### Stimulus Generation

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/stimulus/StimulusManager.cpp/hpp` | `loadConfig()`, `generateSample()` | Manages multiple stimulus sources, mixes signals |
| `src/stimulus/StimulusLua.cpp/hpp` | Lua bindings | Lua scripting interface for stimulus configuration |
| `src/stimulus/ToneGenerator.hpp` | `generate()` | CW/carrier tone generation |
| `src/stimulus/NoiseGenerator.hpp` | `generate()` | Thermal/white noise generation |
| `src/stimulus/MorseGenerator.cpp/hpp` | `setText()`, `generate()` | CW Morse code generation |
| `src/stimulus/SsbGenerator.cpp/hpp` | `generate()` | SSB voice signal generation with Hilbert transform |
| `src/stimulus/SweepGenerator.cpp/hpp` | `generate()` | Frequency sweep generator for testing |
| `src/stimulus/TtsEngine.cpp/hpp` | `speak()` | Text-to-speech using espeak-ng |
| `src/stimulus/AntennaStimulus.hpp` | Base class | Abstract interface for all stimulus types |
| `src/stimulus/RfCapturePlayer.hpp` | `play()` | Plays back recorded IQ files |

### Xyce Integration

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/xyce/XyceWrapper.cpp/hpp` | `initialize()`, `step()`, `getNodeVoltage()` | Wraps Xyce SPICE simulator API |
| `src/orchestrator/Orchestrator.cpp/hpp` | `run()`, `step()` | Coordinates Xyce simulation stepping with ADC sampling |
| `src/sampler/AdcSampler.cpp/hpp` | `sample()` | Extracts QSD node voltages from Xyce at 96kHz |
| `src/sampler/RxControls.hpp` | Control structures | VFO frequency, attenuation settings |

### Transport Layer

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/transport/Transport.hpp` | `ITransport` interface | Abstract transport interface |
| `src/transport/IQFrame.hpp` | `IQFrame` struct | 6-channel I/Q frame structure (3 QSD pairs) |
| `src/transport/SharedMemTransport.cpp/hpp` | `write()`, `read()` | POSIX shared memory for local IPC |
| `src/transport/UnixSocketTransport.cpp/hpp` | Unix sockets | Alternative local transport |
| `src/transport/TcpControlTransport.cpp/hpp` | `listen()`, `accept()`, `sendResponse()` | TCP server for control commands |
| `src/transport/UdpStreamTransport.cpp/hpp` | `start()`, `sendFrame()` | UDP server for IQ frame streaming with CBOR encoding |

### Host-Side Processing (for testing)

| File | Key Functions | Purpose |
|------|---------------|---------|
| `src/host/HostApp.cpp/hpp` | Test harness | Host application framework for testing |
| `src/host/DspPipeline.cpp/hpp` | `process()` | DSP pipeline for host testing |
| `src/host/Visualizer.cpp/hpp` | `display()` | Console-based S-meter display |

### Zephyr Firmware Support

| File | Purpose |
|------|---------|
| `src/zephyr/FirmwareProcess.cpp/hpp` | Launches Zephyr native_sim as subprocess |

### Zephyr Native Simulation

| File | Purpose |
|------|---------|
| `zephyr/CMakeLists.txt` | Zephyr build for native_sim target |
| `zephyr/prj.conf` | Zephyr configuration |
| `zephyr/boards/native_sim.overlay` | Device tree overlay |
| `zephyr/src/main.cpp` | Zephyr main - shell commands, I/Q thread |
| `zephyr/src/virtual_adc.cpp/hpp` | Virtual ADC driver reading from shared memory |
| `zephyr/src/virtual_nco.cpp/hpp` | Virtual NCO driver for frequency control |
| `zephyr/src/twin_transport.cpp/hpp` | Shared memory transport for Zephyr |
| `zephyr/src/iq_frame.hpp` | IQ frame structure (shared with host) |

### Configuration

| File | Purpose |
|------|---------|
| `config/stimuli/default.lua` | Default stimulus configuration script |

### SPICE Netlists

| File | Purpose |
|------|---------|
| `netlists/*.cir` | SPICE circuit definitions for preselector, QSD, transformer models |
| `netlists/*.lib` | SPICE subcircuit libraries |
| `netlists/*.prn` | Simulation output files (generated) |
| `netlists/IMPEDANCE_ANALYSIS.md` | Documentation of impedance domain analysis |

### Protobuf (Optional)

| File | Purpose |
|------|---------|
| `proto/nexrx.proto` | Protocol buffer definitions (optional, -DWITH_PROTOBUF=ON) |

### Test Audio Files

| File | Purpose |
|------|---------|
| `test/CQ-WB7NAB*.wav` | SSB voice recordings for stimulus testing |

### Documentation

| File | Purpose |
|------|---------|
| `README.md` | Comprehensive test documentation |
| `Digital Twin Methodology and Architecture.md` | Architecture overview |
| `IMPLEMENTATION-PLAN.md` | Development plan |

---

## fw/ - STM32 Firmware (OBSOLETE)

**Status:** OBSOLETE - References "NexRig" transceiver project, not NexRx receiver
**Evidence:** CMakeLists.txt references many non-existent source files (RFController.cpp, PowerAmplifier.cpp, etc.)

| File | Status |
|------|--------|
| `CMakeLists.txt` | **OBSOLETE** - References 20+ non-existent source files |
| `src/main.cpp` | **OBSOLETE** - "NexRig" transceiver code with TX/PA concepts not relevant to receiver |
| `fw100tqfp.ioc`, `fw.ioc` | STM32CubeMX project files (may be useful for pin assignments) |
| `include/coms/RestApiHandler.h` | Empty/stub header |
| `include/hw/RfController.h` | Empty/stub header |
| `.settings/*` | Eclipse IDE settings |

**Recommendation:** Delete entire `fw/` directory or archive it separately

---

## mock-server/ - Node.js Mock Server (OBSOLETE)

**Status:** OBSOLETE - References "NexRig" project, uses WebSocket/REST which is replaced by TCP/UDP in twin
**Evidence:** README says "NexRig Mock Server", code references transceiver concepts

| File | Status |
|------|--------|
| `README.md` | Documents NexRig mock server |
| `package.json`, `package-lock.json` | Node.js dependencies |
| `src/server.js` | Express HTTP server |
| `src/iqStreamGenerator.js` | WebSocket I/Q streaming |
| `src/rfController.js` | RF control state |
| `src/systemStatus.js` | Status endpoints |
| `src/cborHandler.js` | CBOR handling |
| `.gitignore`, `.nvmrc` | Node.js config |

**Recommendation:** Delete entire `mock-server/` directory

---

## hw/ - KiCad Hardware Design

**Tool:** KiCad 8.x
**Purpose:** PCB design for NexRx receiver hardware

### Main Design Files

| File | Purpose |
|------|---------|
| `NexRx.kicad_pro` | KiCad project file |
| `NexRx.kicad_sch` | Top-level schematic |
| `NexRx.kicad_pcb` | PCB layout |
| `NexRx.kicad_prl` | Project local settings |

### Hierarchical Schematic Sheets

| File | Purpose |
|------|---------|
| `power.kicad_sch` | Power supply section |
| `microcontroller.kicad_sch` | STM32 microcontroller |
| `fpga.kicad_sch` | ECP5 FPGA section |
| `qsd.kicad_sch` | Triple-QSD mixer |
| `rx-presel.kicad_sch` | Receive preselector/filters |
| `rx-signal-chain.kicad_sch` | RX signal chain |
| `ethernet.kicad_sch` | Ethernet interface |
| `usb-pd.kicad_sch` | USB Power Delivery |
| `tpad.kicad_sch` | T-pad attenuator |

### Libraries

| File | Purpose |
|------|---------|
| `Library.kicad_sym` | Custom schematic symbols |
| `Library.pretty/*.kicad_mod` | Custom footprints (transformers, toroids, connectors, ICs) |
| `fp-lib-table`, `sym-lib-table` | Library path configuration |
| `fp-info-cache` | Footprint cache |

### Reference Files

| File | Purpose |
|------|---------|
| `schematic-20260111.pdf` | Schematic PDF export |
| `pinout-with-alt.csv` | MCU pin assignments |
| `FPGA-SC-02032-2-0-ECP5U-12-Pinout.ods` | FPGA pinout spreadsheet |

### Other Files

| File | Purpose |
|------|---------|
| `Library.bak` | Symbol library backup |
| `replicate_layout.log` | Layout replication log |
| `LICENSE` | Hardware license |
| `.gitignore` | Git ignores |

Note: `NexRx-backups/` exists locally but is already in `.gitignore`.

---

## Build Artifacts Summary

### Currently Active and Functional

| Artifact | Directory | Target | Purpose |
|----------|-----------|--------|---------|
| `nexrx_app` | `app/build/` | Host (Linux/Win/Mac) | **Main GUI application** - connects to twin, displays waterfall, plays audio |
| `setbox_test` | `app/build/` | Host | Unit tests for SetBox configuration engine |
| `twin` | `twin/build/` | Host (Linux) | **Digital twin** - fast functional model, streams IQ to app |
| `twin_xyce` | `twin/build/` | Host (Linux) | Full Xyce SPICE simulation (slow) |
| `stimulus_test` | `twin/build/` | Host (Linux) | Tests stimulus generators |
| `host_test` | `twin/build/` | Host (Linux) | Tests host DSP pipeline |
| `zephyr.exe` | `twin/zephyr/build/` | Host (Linux) | Zephyr firmware in native_sim mode |

### Build Commands

```bash
# App (GUI)
cd app && mkdir -p build && cd build
cmake .. && make -j$(nproc)
# Output: nexrx_app, setbox_test

# Twin (Digital Twin)
cd twin && mkdir -p build && cd build
cmake .. && make -j$(nproc)
# Output: twin, twin_xyce, stimulus_test, host_test

# Zephyr (requires Zephyr SDK)
cd twin/zephyr
source ~/zephyr-projects/zephyr/zephyr-env.sh
west build -b native_sim
# Output: build/zephyr/zephyr.exe
```

### Runtime Configuration

1. Start twin: `./twin/build/twin --functional --stream`
2. Start app: `./app/build/nexrx_app`
3. App connects to twin via TCP:5000 (control) and UDP:5001 (IQ stream)

---

## External Library Dependencies

Explicit libraries the codebase depends on (excluding implicit dependencies like C++ runtime).

### app/ (nexrx_app, setbox_test)

| Library | Purpose | Source |
|---------|---------|--------|
| **SDL2** | Windowing, input, OpenGL context | System (pkg-config or vcpkg) |
| **OpenGL** | Rendering | System |
| **Lua 5.4** | Scripting, UI, configuration | System (pkg-config or vcpkg) |
| **sol2** | C++/Lua binding | FetchContent (GitHub) |
| **tinycbor** | CBOR encode/decode for IQ frames | FetchContent (GitHub) |
| **miniaudio** | Audio output | Header-only (extern/) |
| **stb_truetype** | Font rasterization | Header-only (extern/) |

### twin/ (twin, twin_xyce, stimulus_test, host_test)

| Library | Purpose | Source |
|---------|---------|--------|
| **Lua 5.4** | Stimulus scripting | System (pkg-config) |
| **sol2** | C++/Lua binding | FetchContent (GitHub) |
| **tinycbor** | CBOR encode/decode | FetchContent (GitHub) |
| **FFTW3** | FFT for signal processing | System |
| **espeak-ng** | Text-to-speech for SSB stimulus | System (optional) |
| **LAPACK/BLAS** | Linear algebra (for Xyce) | System |
| **AMD (SuiteSparse)** | Sparse matrices (for Xyce) | System |
| **Xyce** | SPICE simulator | Local build |
| **Trilinos** | Xyce math backend (~30 sub-libs) | Local build |
| **Protobuf** | Message serialization | System (optional, -DWITH_PROTOBUF=ON) |

### Summary by Category

**Core (both app and twin):**
- Lua 5.4, sol2, tinycbor

**App-specific:**
- SDL2, OpenGL, miniaudio, stb_truetype

**Twin-specific:**
- FFTW3, espeak-ng, Xyce/Trilinos (for physics simulation)

---

## Recommendations

### Files/Directories to DELETE (Obsolete)

1. **`fw/`** - Entire directory. Contains "NexRig" transceiver firmware that:
   - References 20+ non-existent source files
   - Has TX/PA concepts not relevant to NexRx receiver
   - Only `main.cpp` exists and it's obsolete code

2. **`mock-server/`** - Entire directory. Contains "NexRig" Node.js server that:
   - Uses WebSocket streaming (replaced by UDP in twin)
   - Uses REST API (replaced by TCP commands)
   - No longer needed since twin provides simulation

### Files to Review

1. **`twin/netlists/*.prn`** - Generated simulation outputs, could be gitignored

### Directory Structure After Cleanup

```
NexRx/
├── app/                    # Host GUI application
│   ├── src/               # C++ source
│   ├── lua/               # Lua UI scripts
│   ├── config/            # SetBox configuration
│   ├── codegen/           # Code generation
│   └── extern/            # Header-only libraries
├── twin/                   # Digital twin simulation
│   ├── src/               # C++ source
│   ├── config/            # Stimulus configs
│   ├── netlists/          # SPICE circuits
│   └── zephyr/            # Zephyr native_sim
├── hw/                     # KiCad hardware design
│   ├── *.kicad_sch        # Schematics
│   ├── *.kicad_pcb        # PCB layout
│   └── Library.pretty/    # Custom footprints
├── doc/                    # Documentation
├── datasheets+doc/         # Component datasheets
└── tools/                  # Development tools
```

### Recommended Immediate Actions

1. `rm -rf fw/` - Remove obsolete firmware directory
2. `rm -rf mock-server/` - Remove obsolete mock server
3. Add `twin/netlists/*.prn` to `.gitignore`

---

*Generated by Claude Code*
