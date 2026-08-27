# NexRx Software & Digital Twin Architecture Specification

## 1. System Overview & Partitioning

The NexRx software ecosystem is partitioned into three distinct
operational domains: the **STM32 Embedded Firmware**, the **PC Host
Receiver Engine**, and the **Hardware Digital Twin (`Twin`)**. The
design enforces a strict separation of concerns, offloading real-time
DSP, phase-coherent decimation, and hardware sequencing to embedded
execution while dedicating host resources to high-level filtering,
demodulation, and stream management.

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                             PC Host Application                               │
│  ┌────────────────────────┐   ┌────────────────────┐   ┌───────────────────┐  │
│  │   USB Ingest Engine    │   │ Host-Side ASRC     │   │ Channel Demod     │  │
│  │ (WinUSB/libusb Stream) │─▶│ & Elastic Buffer   │─▶│ (SSB/CW/AM/Modes) │  │
│  └────────────────────────┘   └────────────────────┘   └───────────────────┘  │
│               ▲                                                 │             │
│               │ (2-ch 384ksps I/Q @ 24.576Mbps)                 │             │
└───────────────┼─────────────────────────────────────────────────┼─────────────┘
                |                                                 │
  [USB Bulk Pipe / IPC Socket]                         [High-Level Control Pipe]
                |                                                 │
┌───────────────┴─────────────────────────────────────────────────▼───────────┐
│                          Shared Firmware Core (C++)                         │
│  ┌────────────────────────┐  ┌────────────────────┐  ┌───────────────────┐  │
│  │ DSP Pipeline Engine    │  │ VFO State Machine  │  │ Autonomous BIST   │  │
│  │ (2x4 Matrix, Rot, Sum) │  │ & Mode Hysteresis  │  │ & Goertzel Engine │  │
│  └────────────────────────┘  └────────────────────┘  └───────────────────┘  │
└───────────────────────────────────────┬─────────────────────────────────────┘
                                        │
                         ┌──────────────┴──────────────┐
                         ▼                             ▼
┌───────────────────────────────────┐ ┌────────────────────────────────┐
│     STM32 Physical Platform       │ │    PC Hardware Digital Twin    │
├───────────────────────────────────┤ ├────────────────────────────────┤
│ • STM32 HAL / SAI / I2C / USB     │ │ • HalSynth (Simulated LO)      │
│ • Dual TLV320ADC5140 TDM Ingest   │ │ • HalAudio (Synthetic RF Gen)  │
│ • Si5351 Clock Synthesizer        │ │ • HalTransport (Socket / Pipe) │
│ • CPLD Shift Registers & Relays   │ │ • SamplePacer (384ksps Timer)  │
└───────────────────────────────────┘ └────────────────────────────────┘
```

---

## 2. Shared Firmware Core & Hardware Abstraction Layer (HAL)

To guarantee exact behavioral parity between physical silicon and the
simulation environment without duplicating firmware logic, all core
receiver algorithms compile against C++ Hardware Abstraction
interfaces (see `HAL*.h`). All interface filenames and class
definitions strictly use `PascalCase`, omitting underscores.

---

## 3. STM32 Embedded Firmware Pipeline

### 3.1 DMA Audio Ingest & Polyphase Recombination

* **Physical Ingest:** The STM32 SAI peripheral receives 8
  differential channels across synchronous TDM streams from two
  TLV320ADC5140 converters at 384ksps of 24-bit samples.


* **Intrinsic Harmonic Rejection:** For each 8-way Octal Sampling
  Detector (OSD0 and OSD1), a programmable $2 \times 4$ projection
  matrix converts the 4 differential pairs into orthogonal $I$ and $Q$
  baseband signals, rejecting 3rd and 5th odd harmonics by
  $>60\text{dB}$:



$$I = \text{CH}_1 + \frac{\sqrt{2}}{2}\text{CH}_3 - \frac{\sqrt{2}}{2}\text{CH}_4$$


$$Q = \text{CH}_2 + \frac{\sqrt{2}}{2}\text{CH}_3 + \frac{\sqrt{2}}{2}\text{CH}_4$$


* **Calibration Weighting:** The fixed geometric coefficients
  $\frac{\sqrt{2}}{2} \approx 0.7071$ are dynamically modified by
  calibration offsets stored in non-volatile memory to cancel physical
  analog board imbalances.



### 3.2 Dual-OSD Translation & Coherent Summation

* **Spectral Rotation:** The OSD0 baseband stream is shifted up by
  $+12\text{kHz}$ and the OSD1 baseband stream is shifted down by
  $-12\text{kHz}$ via complex exponential multiplication ($e^{\pm
  j\omega t}$) to center the true RF spectrum at baseband.


* **Vector Summation:** The firmware adds the rotated streams together
  in the time domain, producing a $+6\text{dB}$ constructive
  reinforcement of true RF signals while unaligned hardware images, LO
  spurs, and ADC artifacts are vetoed into the noise floor.


* **DC Hole Mitigation:** Offsetting the local oscillators by $\pm
  12\text{kHz}$ places the physical DC notches of the analog front
  end outside the primary detection passband, eliminating the central
  DC spike without requiring heavy host-side notch filtering.



### 3.3 VFO State Machine & 15.1 MHz Dual-Mode Hysteresis

* **Conversion Gain Compensation:** Polyphase 8-way commutation yields
  a $-0.9\text{dB}$ conversion loss, whereas high-band 4-way Tayloe
  commutation exhibits a $-3.9\text{dB}$ loss. When transitioning to
  4-way mode above 15.1 MHz, the STM32 applies a frame-synchronous
  $+3\text{dB}$ digital gain compensation to maintain a stable
  baseline noise floor.


* **Viewport Hysteresis Window:** The mode transition is governed by a
  300 kHz hysteresis loop (14.950 MHz to 15.250 MHz) tied to the
  active display span, ensuring hardware state transitions never occur
  while the 15.1 MHz boundary is within the active passband.


* **Deterministic Tuning Sequence:**
1. Drive `OutputEnableBar` (OEB) HIGH to halt clock outputs.


2. Write new fractional divider parameters to the Si5351 via I2C.


3. Trigger a soft PLL reset on Si5351 Register 177.


4. Enforce a 1.0 ms settling delay for analog VCO lock.


5. Pulse `CPLDReset` to align walking ring counters to state
   `00000001`.


6. Drive `OutputEnableBar` LOW to simultaneously launch phase-locked
   clocks into the mixers.





### 3.4 Autonomous Scalar Network Analyzer (SNA) BIST Engine

* **Closed-Loop Test Execution:** An on-chip diagnostic state machine
  runs a 113-bin frequency sweep (1.8 MHz to 30.0 MHz in 250 kHz
  steps).


* **Tracking Clock Generation:** Si5351 CH0 outputs a test tone at
  $f$, while CH1 outputs an OSD tracking clock at $4(f + 10\text{
  kHz})$.


* **Goertzel Magnitude Filter:** For each 1.0 ms step (384 samples),
  the STM32 executes a 10 kHz Goertzel magnitude detector on the
  polyphase-recombined baseband stream.


* **ATE Data Export:** The final 113-element magnitude array is
  serialized and output over the USB Virtual COM Port (VCP) as a
  standard CSV/JSON payload.



---

## 4. Hardware Digital Twin (`Twin`)

The `Twin` subsystem models the physical hardware and executes the
identical shared C++ firmware algorithms on the host PC, validating
downstream DSP and host communication paths before physical hardware
assembly.

### 4.1 Architecture & Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Digital Twin (`Twin`) Execution                    │
│                                                                             │
│  ┌───────────────────────┐                     ┌─────────────────────────┐  │
│  │ Synthetic RF Gen      │                     │ Twin Synth Model        │  │
│  │ (Carriers, Noise, QRN)│                     │ (Calculates LO Freqs    │  │
│  └──────────┬────────────┘                     │  from Si5351 Registers) │  │
│             │                                  └────────────┬────────────┘  │
│             ▼                                               │               │
│  ┌──────────────────────────────────────────────────────────┴────────────┐  │
│  │ Simulated Mixer & ADC Downconversion (OSD0 / OSD1 Commutation)        │  │
│  └──────────────────────────────────────┬────────────────────────────────┘  │
│                                         │                                   │
│                                         ▼ (8 Differential Channels)         │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ Shared Firmware DSP Core:                                             │  │
│  │ • 2x4 Polyphase Matrix Multiplication                                 │  │
│  │ • NCO Frequency Shifts (+/- 12 kHz)                                   │  │
│  │ • Coherent Summation & Image Vetoing                                  │  │
│  │ • Dual-Mode Gain Normalization (+3 dB)                                │  │
│  └──────────────────────────────────────┬────────────────────────────────┘  │
│                                         │                                   │
│                                         ▼ (2-ch Normalized I/Q)             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ SamplePacer & Virtual Transport (IPC Socket / Named Pipe @ 384ksps)   │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘

```

### 4.2 Synthetic RF & Mixer Simulation

* **RF Signal Synthesis:** Mathematical signal generators synthesize
  arbitrary RF test environments:

$$S_{\text{RF}}(t) = \sum_{k} A_k \cos(2\pi f_k t + \phi_k) + N_{\text{AWGN}}(t) + N_{\text{Impulse}}(t)$$


* **Simulated LO Tracking:** The `TwinSynth` class intercepts register
  writes to Si5351 MultiSynth parameters and computes the active
  mixing frequencies for OSD0 and OSD1.
* **Commutation Simulation:** The RF input is downconverted by
  multiplying against simulated 8-phase (or 4-phase) switching
  functions, generating 8 differential baseband audio channels with
  simulated analog phase/amplitude skews to enable testing of skew
  compensation algorithms.

### 4.3 Deterministic Sample Pacing

* **Master Clock Authority:** The `Twin` operates as an unyielding
  data master, pumping samples at precisely 384ksps.

* **High-Resolution Pacer:** A dedicated `SamplePacer` thread uses
  high-precision timers (e.g., `std::chrono::steady_clock`) to push
  384 sample pairs every 1.0 ms into the virtual transport.
* **Zero Backpressure:** Transport endpoints operate in non-blocking
  mode; if the host application fails to read incoming blocks in real
  time, the virtual transport buffer overflows, faithfully reproducing
  physical USB DMA overrun behavior.

---

## 5. Host PC Software Architecture

The host PC receiver engine ingests the normalized baseband stream,
resolves clock domain drift, manages adaptive noise cancellation, and
executes mode demodulation.

### 5.1 Driverless USB Transport Layer

* **Protocol:** Vendor-Specific Bulk USB Class utilizing Microsoft OS
  2.0 Descriptors.


* **Platform Binding:**
* **Windows 10/11:** Automatic native binding to `WinUSB.sys` without
  administrative prompts or third-party filter drivers.


* **macOS:** User-space bulk communication via `libusb` / `IOUSBHost`.


* **Linux:** User-space access via standard non-root `udev`
  permissions (`99-nexrx.rules`).




* **Payload Ingest:** Continuously receives a 2-channel, 24-bit
  baseband stream packed into 32-bit words at 384ksps (24.576Mbps
  sustained throughput).



### 5.2 Host-Side Elastic Buffering & Dynamic Time Interpolation

Because the physical STM32 ADC crystal and the host PC sound card DAC
operate on independent physical timebases, the host software
implements host-side sample rate adaptation to prevent FIFO overflow
or starvation.

```
[Incoming USB Bulk Stream / Twin IPC (384ksps Master)]
                         │
                         ▼
[Host Ingest Ring Buffer (Lock-Free FIFO)]
                         │
                         ├────────────────────────────────────────┐
                         ▼                                        ▼
           [Watermark Tracking / Software PLL]         [Baseband DSP Processing]
                         │                             (LMS Noise Filter, Demod)
                         ▼                                        │
           [Fractional ASRC / Interpolator]                       ▼
                         │                             [Audio Output Buffer]
                         └────────────────────────────────────────┤
                                                                  ▼
                                                      [OS Audio DAC Master Clock]
                                                      (WASAPI / CoreAudio / ALSA)

```

* **Ingest Ring Buffer:** Incoming bulk packets are pushed immediately
  into a high-capacity lock-free ring buffer.
* **Buffer Watermark Tracking:** A software tracking loop monitors the
  long-term fill level of the buffer against the local audio
  consumption rate.
* **Asynchronous Sample Rate Conversion (ASRC):** A high-order
  polynomial (or Farrow) fractional interpolator dynamically adjusts
  the resampling ratio by minute fractions of a percent, eliminating
  buffer underruns and overruns without dropping samples or
  introducing audible pitch artifacts.
* **Concealment Engine:** If severe operating system scheduling stalls
  occur, the host applies Vector Blanking and Linear Predictive Coding
  (LPC) interpolation to patch data discontinuities seamlessly.



### 5.3 Demodulation & Fundamental Signal Chain

* **Panoramic Waterfall & Panadapter:** The host computes direct FFTs
  on the incoming pre-stitched, normalized 384ksps $I/Q$ stream,
  displaying a 300 kHz panoramic view with zero DC spike or image
  artifacts.


* **Digital Down-Conversion (DDC):** A software NCO and complex mixer
  translate the desired listening frequency within the 300 kHz
  passband down to zero IF.
* **Demodulation Blocks:**
* **SSB/CW:** Weaver / Phasing method baseband filtering with variable
  bandwidth selection.
* **AM:** Synchronous carrier recovery and envelope detection.
* **Digital Modes:** Baseband routing to external virtual audio
  endpoints for digital decoding.


* **Adaptive Filtering:** Multi-tap Least Mean Squares (LMS) adaptive
  noise reduction and automatic notch filtering operating on the
  extracted audio channel.



---

## 6. Interface & Wire Protocol Specification

### 6.1 USB Bulk Endpoint Architecture

| Endpoint | Direction | Transfer Type | Max Packet Size | Description |
| --- | --- | --- | --- | --- |
| `0x00` | Bidirectional | Control | 64 Bytes | Standard enumeration & MS OS 2.0 descriptors

 |
| `0x81` | IN (MCU $\to$ Host) | Bulk | 512 Bytes | Normalized 2-Channel $I/Q$ data stream (384ksps)

 |
| `0x02` | OUT (Host $\to$ MCU) | Bulk | 64 Bytes | High-level receiver command channel

 |
| `0x83` | IN (MCU $\to$ Host) | Bulk / Interrupt | 64 Bytes | Telemetry, state feedback & SNA sweep responses

 |

### 6.2 Data Stream Framing (`0x81` Bulk IN)

* **Sample Depth:** 24-bit signed integer, sign-extended and
  left-aligned into a 32-bit signed integer word.

* **Channel Interleaving:** Consecutive sample pairs are transmitted
  in alternating $[I_k, Q_k]$ sequence:

$$\text{Payload Word: } [\text{Sample } k \text{ (Channel 0 / } I
\text{)}], [\text{Sample } k \text{ (Channel 1 / } Q \text{)}]$$

* **Data Rate:**

$$2 \text{ channels} \times 32 \text{ bits/sample} \times 384,000
\text{ samples/sec} = 24.576 \text{Mbps}$$


### 6.3 Command Protocol Structure (`0x02` Bulk OUT)

Commands are structured as fixed-header, variable-length binary
packets, where the `n` byte structure is:

	00: Sync Byte (0xA5)
	01: Command ID
	02: Payload Length
	03..n-3: Payload Data (n-5 bytes)
	n-1..n-2: CRC-16-CCITT (16-bits)

#### Core Command Set

* `0x0001` - `SetTunedFrequency`: Payload contains a 32-bit unsigned
  integer representing target center frequency in Hz.


* `0x0002` - `SetAnalogGain`: Payload contains an 8-bit unsigned
  integer setting front-end PGA gain (0 to 42 dB).


* `0x0003` - `SetAttenuator`: Payload contains an 8-bit value in dB to
  specify the input T-pad attenuation required.


* `0x0010` - `TriggerRFSweep`: Payload defines SNA start frequency,
  stop frequency, and step size for autonomous BIST execution.


* `0x0020` - `SetRecombinationWeights`: Payload contains eight 16-bit
  fixed-point coefficients for the $2 \times 4$ polyphase
  recombination matrix.
