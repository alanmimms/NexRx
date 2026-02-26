# NexRx Digital Twin Architecture

## 1. Overview
The NexRx Digital Twin is a Software-in-the-Loop (SiL) simulation environment that allows the host application (DSP, UI) to be developed, tested, and validated without requiring physical hardware. It accurately emulates the RF frontend, the unique Triple-QSD (Quadrature Sampling Detector) architecture, and the network transport mechanisms of the physical radio.

The Twin operates primarily in a **High-Fidelity Functional Mode**, which uses complex analytic mathematics to synthesize and mix signals in real-time, avoiding the aliasing artifacts common in discrete-time RF simulations.

---

## 2. Signal Pipeline

### 2.1 RF Stimulus Generation
The Digital Twin uses a Lua-scriptable `StimulusManager` to inject RF signals into the virtual antenna.
- **Supported Signals:** CW (Morse), SSB (Multi-tone or Audio Files), Sweeps, Thermal Noise, and I/Q RF Capture playback.
- **Complex Baseband Synthesis:** Instead of generating real-valued voltages at high RF frequencies, generators compute the complex envelope `(I + jQ)` of the signal at its target frequency using absolute time. This analytic approach guarantees mathematically perfect frequency representation without Nyquist aliasing issues.

### 2.2 High-Fidelity Analytic Mixing
The core of the simulation is the virtual QSD array.
- The composite RF analytic signal is complex-downconverted by three independent VFOs representing the three hardware QSDs.
- `baseband = (rf_i + j*rf_q) * exp(-j * 2pi * vfo * t)`
- This continuous-time equivalent mixing occurs before any decimation, providing pristine downconversion regardless of how high the carrier frequency is.

### 2.3 Internal Signal Generator (ISG)
The twin simulates the hardware's internal FPGA-based PDM signal generator (used for calibration and testing). The ISG signal is injected into the processing chain such that its amplitude remains constant relative to the ADC, regardless of the RF attenuator settings, perfectly mimicking the hardware injection topology.

### 2.4 Filtering, Decimation, and Quantization
- Baseband signals are passed through a chain of Biquad low-pass filters that model the AK5578 ADC's internal anti-aliasing response.
- Signals are decimated from the internal oversampled simulation rate (e.g., 480 kHz) to the target output sample rate (96 kHz).
- Finally, the high-precision floating-point values are dithered with uniform noise and quantized to 24-bit integers to accurately emulate the ADC's dynamic range and noise floor.

---

## 3. Host Application Interface

The app connects to the Digital Twin over two virtual network links, exactly mimicking the physical hardware's NexBus interface over Ethernet/USB.

### 3.1 Control Plane (TCP Port 5000)
A text-based command protocol allowing the app to control the hardware state.
- `SET_QSD_VFO <ch> <freq_hz>`: Tunes one of the three QSDs. The app offsets QSD 0 and 1 by `+k` and `-k` kHz.
- `SET_ATTEN_TOTAL <db>`: Adjusts the RF front-end attenuation.
- `SET_PRESEL_C <idx> <0|1>`: Toggles individual preselector capacitor relays.
- `SET_PRESEL_L <0|1>`: Toggles the preselector inductor relay.
- `SET_BIST_ENABLE <0|1>`: Enables the Internal Signal Generator (ISG).
- `SET_BIST_FREQ <freq_hz>`: Sets the ISG frequency.

### 3.2 Data Stream (UDP Port 5001)
A high-speed data pipe sending interleaved I/Q samples to the host.
- Sent in batches of `IQFrame` structs.
- Each frame contains a timestamp, a sequence number, and 3 pairs of 32-bit (containing 24-bit data) I/Q values representing the simultaneous state of the three QSDs.

---

## 4. Hardware Emulation Parity

Currently, the twin effectively matches the hardware in several critical areas:
- **Tuning & VFO control:** The app issues separate VFO commands for the 3 QSDs, driving them to independent frequencies.
- **Data formatting:** The app receives the exact binary `IQFrame` structures it will receive from the STM32H7 via Ethernet/USB.
- **Signal routing & DSP:** The app performs its LMS adaptive interference cancellation and baseband filtering using the 3-channel data just as it will with real hardware.
- **ISG/Attenuator topology:** Changing attenuation affects the apparent strength of external signals but leaves the ISG calibration tone unaffected.

---

## 5. Future Work & Emulation Completeness

To make the emulation a perfect 1:1 representation of the physical world, several milestones remain:

1. **Harmonic Mixing Simulation:** The current analytic mixer only downconverts the fundamental VFO frequency. Real QSDs act as square-wave mixers, heavily mixing on odd harmonics (3rd, 5th, etc.). The twin needs to generate these harmonic responses so the app's 1-2-1 harmonic cancellation logic can be validated.
2. **Zephyr Firmware Surrogate:** Transition from the standalone C++ `twin` application to running the actual STM32H7 C code in a Zephyr `native_sim` environment. The firmware will use POSIX shared memory to pull samples from the C++ signal generator.
3. **Xyce Physics Engine Integration:** For true analog validation, complete the integration of the Xyce SPICE simulator. This will allow testing of the exact non-linearities of the TS3A4751 CMOS switches, switch charge injection, and the mutual inductance of the hexafilar transformer, rather than relying solely on idealized mathematical mixing.
4. **Preselector Physics:** Currently, the preselector commands (`SET_PRESEL_C`, etc.) are accepted but do not actively filter the RF spectrum in the fast Functional Mode. This requires implementing a dynamic filter whose coefficients update based on the L/C relay states.
5. **A Priori Calibration Support:** Implement a file transfer mechanism to allow the app to generate and upload preselector and attenuator calibration tables to the twin's virtual flash filesystem, matching the STM32H7's behavior.
