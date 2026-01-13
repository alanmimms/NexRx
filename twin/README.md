# NexRx Digital Twin

Software-in-the-Loop simulation environment for the NexRx Triple-QSD SDR receiver.

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

For Zephyr firmware (requires Zephyr SDK):
```bash
cd zephyr
source ~/zephyr-projects/zephyr/zephyr-env.sh
west build -b native_sim
```

## Test Executables

### 1. stimulus_test

**Purpose:** Validates the antenna stimulus generators used to inject test signals into the simulation.

**Run:**
```bash
./build/stimulus_test
```

**What it tests:**
- `ToneGenerator` - Single/multi-tone CW signals
- `NoiseGenerator` - Thermal noise generation
- `CompositeStimulus` - Signal + noise combinations
- `RfCapturePlayer` - I/Q file playback with upconversion

**Expected output:**
```
=== ToneGenerator Tests ===
1. CW 14.010 MHz, 100.0 mV peak
   Peak: 100.0 mV (expected 100 mV)
   RMS:  70.7 mV (expected 70.7 mV)
   Freq: 14.01 MHz (expected 14.01 MHz)

2. Two-tone 14.000 MHz +/- 500 Hz
   Peak: ~100 mV when in phase
   ...

=== NoiseGenerator Tests ===
1. White noise 1.0 uV RMS
   Measured RMS: 1.0 uV (expected 1.0 uV)

2. Thermal noise (290K, 3kHz BW, 50 ohm)
   Measured RMS: ~155 nV (expected ~155 nV)
   ...

=== All Tests Complete ===
```

**Pass/Fail criteria:**
- **PASS:** All measured values within 10% of expected values
- **FAIL:** Large deviations indicate bugs in signal generation math
- Key checks:
  - CW tone peak should be exactly as specified
  - RMS should be peak/sqrt(2) for sinusoids
  - Thermal noise should match Johnson-Nyquist formula
  - S-meter levels: S9 = 50uV, each S-unit = 6dB


### 2. host_test

**Purpose:** Tests the host-side DSP pipeline and visualization without requiring Xyce simulation.

**Run:**
```bash
./build/host_test              # Synthetic test signal
./build/host_test --shm NAME   # Connect to running twin
```

**What it tests:**
- Synthetic QSD mixing model (RF x LO quadrature)
- `DspPipeline` signal processing
- `Visualizer` S-meter and level display
- Baseband frequency estimation

**Expected output (synthetic mode):**
```
=== NexRx Host Test - Synthetic Signal ===
RF: 14.010 MHz, 1mV peak
LO: 14.000 MHz
Expected baseband: 10 kHz

Processing 9600 samples...

S-Meter: [####      ] S5
Level: -73.2 dBm

=== Signal Analysis ===
Estimated baseband freq: 10.0 kHz
RMS level: 0.25 mV
Level: -73 dBm
S-meter: S5
```

**Pass/Fail criteria:**
- **PASS:**
  - Estimated baseband frequency within 5% of expected (10 kHz)
  - S-meter reading reasonable for 1mV input (S4-S6 range)
  - No NaN or infinite values
- **FAIL:**
  - Baseband frequency wildly wrong (indicates mixing bug)
  - Zero output (indicates signal path broken)
  - S-meter shows S0 or S9+60 (indicates scaling bug)


### 3. twin

**Purpose:** Digital twin signal generator and end-to-end test of the RF simulation pipeline. Two modes:
- `--functional`: Fast C++ model, runs real-time, streams I/Q to app
- Default: Full Xyce SPICE simulation (accurate but slow)

**Run:**
```bash
# Fast functional mode with streaming (for app integration)
./build/twin --functional --stream

# Full Xyce physics simulation
./build/twin --duration 1 --netlist netlists/pipeline_test.cir
```

**What it tests:**
- Xyce initialization and simulation stepping
- ADC sampling callback at 96 kHz
- Node voltage extraction from SPICE
- I/Q frame generation and storage

**Expected output:**
```
=== NexRx Digital Twin Pipeline Test ===
Netlist: netlists/pipeline_test.cir
Duration: 1.0 ms

--- Initializing Xyce ---
[Xyce initialization messages]

--- Running Simulation ---

--- Results ---
Success: yes
Samples collected: 96
Wall time: 2.5 s
Speed: 0.0004x realtime

--- First 10 samples ---
t=    10.417us Q0:   0.123/  -0.456mV Q1:   0.123/  -0.456mV Q2:   0.123/  -0.456mV
...

--- Signal Analysis (QSD0) ---
RMS magnitude: 0.5 mV
Expected baseband: 10 kHz
```

**Pass/Fail criteria:**
- **PASS:**
  - "Success: yes"
  - Sample count matches expected (duration * 96000)
  - No samples with error flag (flags != 0)
  - RMS magnitude > 0 (signal present)
- **FAIL:**
  - "Success: no" (Xyce error)
  - Zero samples collected
  - All samples show 0/0 values (node names wrong)
  - Error flags set on frames

**Note:** Requires a valid netlist with nodes named Q0_I, Q0_Q, Q1_I, Q1_Q, Q2_I, Q2_Q.


### 4. Zephyr Firmware (zephyr.exe)

**Purpose:** Tests the Zephyr RTOS firmware running on native_sim (Linux x86_64).

**Run:**
```bash
./zephyr/build/zephyr/zephyr.exe
```

**What it tests:**
- Zephyr RTOS boot and initialization
- Virtual NCO peripheral (frequency setting)
- Virtual ADC peripheral (I/Q frame reading)
- POSIX shared memory connection
- Shell command interface

**Expected output:**
```
uart connected to pseudotty: /dev/pts/X
*** Booting Zephyr OS build v4.3.0 ***
[00:00:00.000,000] <inf> nexrx_main: NexRx Digital Twin Firmware starting...
[00:00:00.000,000] <inf> nexrx_main: Build: Jan  5 2026 15:54:37
[00:00:00.000,000] <inf> twin_transport: Initializing twin transport
[00:00:00.000,000] <inf> twin_transport:   SHM: /nexrx_iq
[00:00:00.000,000] <wrn> twin_transport: Failed to open shared memory: /nexrx_iq (errno=22)
[00:00:00.000,000] <err> nexrx_main: Failed to initialize transport
[00:00:00.000,000] <inf> virtual_nco: Virtual NCO initialized with 3 channels
[00:00:00.000,000] <inf> virtual_adc: Virtual ADC initialized
[00:00:00.000,000] <inf> virtual_adc:   Channels: 6
[00:00:00.000,000] <inf> virtual_adc:   Sample rate: 96000 Hz
[00:00:00.000,000] <inf> virtual_adc:   Resolution: 24 bits
[00:00:00.000,000] <inf> virtual_nco: NCO0 frequency set to 14000000 Hz
[00:00:00.000,000] <inf> virtual_nco: NCO1 frequency set to 14000000 Hz
[00:00:00.000,000] <inf> virtual_nco: NCO2 frequency set to 14000000 Hz
[00:00:00.000,000] <inf> nexrx_main: I/Q processing thread started
[00:00:00.000,000] <inf> nexrx_main: Firmware initialized, entering shell
```

**Pass/Fail criteria:**
- **PASS:**
  - "Booting Zephyr OS" message appears
  - All three NCOs initialized to 14 MHz
  - Virtual ADC shows 6 channels, 96kHz, 24-bit
  - "Firmware initialized, entering shell"
  - Shared memory warning is OK when orchestrator not running
- **FAIL:**
  - Crash or segfault
  - Missing NCO or ADC initialization messages
  - Boot loop or hang before "Firmware initialized"

**Shell commands (interactive):**
```
uart:~$ nexrx status     # Show VFO frequencies and frame count
uart:~$ nexrx freq 0 7000000   # Set VFO0 to 7 MHz
```


## Architecture Overview

```
                    ┌─────────────────┐
                    │  AntennaStimulus │ ◄── stimulus_test
                    │  (ToneGenerator) │
                    └────────┬────────┘
                             │ RF signal
                             ▼
┌────────────────────────────────────────────────────────────┐
│                     Xyce SPICE Simulation                   │ ◄── twin
│  (Preselector → Transformer → Triple-QSD → TIA)            │
└────────────────────────────┬───────────────────────────────┘
                             │ Node voltages
                             ▼
                    ┌─────────────────┐
                    │   AdcSampler    │ @ 96 kHz
                    └────────┬────────┘
                             │ IQFrame
                             ▼
                    ┌─────────────────┐
                    │ SharedMemTransport│
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
              ▼                             ▼
     ┌─────────────────┐           ┌─────────────────┐
     │  Zephyr Firmware │           │    Host App     │ ◄── host_test
     │  (native_sim)   │           │  (DspPipeline)  │
     └─────────────────┘           └─────────────────┘
              ▲
              │
      zephyr.exe test
```

## Quick Validation

Run all tests in sequence:
```bash
cd build

# 1. Stimulus generators (no external deps)
./stimulus_test

# 2. Host DSP pipeline (no external deps)
./host_test

# 3. Digital twin - functional mode (fast)
./twin --functional --duration 100

# 3b. Or full Xyce simulation (slow, requires Xyce)
./twin --duration 0.1 --netlist netlists/pipeline_test.cir

# 4. Zephyr firmware
../zephyr/build/zephyr/zephyr.exe &
sleep 2
kill %1
```

All tests should complete without crashes. Check output against criteria above.


## Dependencies

- CMake 3.20+
- C++20 compiler (GCC 11+ or Clang 14+)
- Xyce 7.10 (for twin physics mode)
- Trilinos libraries (for Xyce)
- Zephyr SDK 4.3+ (for firmware)
- FFTW3, LAPACK, BLAS
