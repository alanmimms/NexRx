# NexRx Digital Twin Architecture

## 1. Overview

The NexRx Digital Twin (`Twin`) is a Software-in-the-Loop (SiL)
simulation environment that models the physical hardware and executes
the identical shared C++ firmware algorithms natively on the host
PC. It allows the host PC receiver engine (DSP, UI, demodulation) to
be developed, tested, and validated without requiring physical
hardware.

The Twin accurately emulates the RF frontend's Dual Octal Sampling
Detector (OSD) architecture and runs the exact STM32 embedded DSP
pipeline to synthesize the final data stream.

---

## 2. Shared Firmware Core & Hardware Abstraction Layer (HAL)

To guarantee exact behavioral parity between the physical silicon and
the simulation environment without duplicating firmware logic, all
core receiver algorithms compile against abstract C++ Hardware
Abstraction interfaces.

* **HALSynth:** The Twin intercepts simulated I2C register writes to
  Si5351 MultiSynth parameters and computes the active mixing
  frequencies ($f - 12\text{kHz}$ and $f + 12\text{kHz}$) for OSD0
  and OSD1.

* **HALGpio:** Simulates the state of hardware control pins, including
  the Output Enable Bar (OEB), reset lines, and mode selection relays.

* **HALAudio:** Injects synthetic RF test vectors into the simulated
  mixing and ADC quantization stages, pushing the resulting 8-channel
  frames to the DSP callback.

* **HALTransport:** Binds the high-level command and data streams to a
  local Inter-Process Communication (IPC) transport—such as a POSIX
  domain socket or named pipe—acting as a direct stand-in for the
  physical USB endpoints.

---

## 3. Simulated Signal Pipeline

### 3.1 Synthetic RF Generation

Mathematical signal generators synthesize arbitrary RF test
environments. The engine supports complex carrier injection,
atmospheric noise (AWGN), QRN impulses, and modulated digital signals.

### 3.2 Mixer & ADC Behavioral Engine

* The synthetic RF input is downconverted by multiplying it against
  simulated 8-phase or 4-phase switching functions.

* This generates the 8 differential baseband audio channels while
  simulating analog phase and amplitude skews typical of the physical
  hardware.


### 3.3 Deterministic Sample Pacing

* The Twin acts as an unyielding data master, operating at precisely
  384ksps.

* A dedicated `SamplePacer` thread uses high-precision timers to push
  exactly 384 sample pairs every 1.0 ms into the virtual transport.

* This perfectly mimics the timing of the STM32's hardware DMA buffer
  intervals and forces the host software to perform host-side elastic
  buffering and dynamic time interpolation.

---

## 4. Embedded DSP Pipeline (Executed in the Twin)

Rather than executing pre-processing on the host, the Twin runs the
exact embedded DSP pipeline to condense the 8 differential channels
internally before transmission.

* **Polyphase Recombination:** Multiplies the incoming channels by a
  programmable $2 \times 4$ projection matrix to produce orthogonal
  $I$ and $Q$ baseband signals, intrinsically rejecting 3rd and 5th
  harmonics.

* **Frequency Shift & Image Vetoing:** Rotates the OSD0 stream by
  $+12\text{kHz}$ and the OSD1 stream by $-12\text{kHz}$. The
  streams are then summed coherently to achieve a $+6\text{dB}$
  signal reinforcement while vetoing unaligned hardware images and ADC
  spurs into the noise floor.

* **Hardware State Encapsulation (Mode Gain Normalization):**
  Evaluates the 15.1 MHz viewport hysteresis to manage 8-way vs. 4-way
  simulated hardware switching. It automatically applies a $+3\text{
  dB}$ digital gain scaling when 4-way mode is active to keep the
  baseline noise floor perfectly stable.


* **Autonomous BIST Engine:** Executes the 113-bin Goertzel magnitude
  detector for closed-loop Scalar Network Analyzer (SNA) sweeps across
  1.8 to 30.0MHz.

---

## 5. Host Application Interface

The host PC application connects to the Twin via virtual links that
perfectly replicate the data formatting and payload bandwidth of the
physical USB interfaces.

### 5.1 Command Protocol (Virtual Endpoint 0x02 OUT)

The host issues high-level VFO and state commands, completely
decoupled from the low-level hardware orchestration. Commands are
fixed-header, variable-length binary packets.

* `0x0001 - SetTunedFrequency`: A 32-bit unsigned integer representing
  the target center frequency in Hz.

* `0x0002 - SetAnalogGain`: An 8-bit unsigned integer setting
  front-end PGA gain from 0 to 42 dB.

* `0x0003 - SetAttenuator`: Configures the input T-pad attenuation
  states.

* `0x0010 - TriggerRFSweep`: Defines the start, stop, and step size
  for autonomous SNA BIST execution.

* `0x0020 - SetRecombinationWeights`: Uploads fixed-point coefficients
  to calibrate the $2 \times 4$ polyphase recombination matrix.

### 5.2 Data Stream Framing (Virtual Endpoint 0x81 IN)

The Twin delivers a single, continuous, and normalized pre-stitched
$I/Q$ baseband stream requiring 24.576 Mbps of sustained throughput.

* **Sample Depth & Alignment:** 24-bit signed integers, sign-extended
  and left-aligned into 32-bit integer words.

* **Interleaving:** Consecutive sample pairs are transmitted in an
  alternating $[I_k, Q_k]$ sequence.
