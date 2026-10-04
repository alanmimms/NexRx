# NexRx Firmware (fw) - Development TODO

## Phase 1: Basic Bring-Up & Infrastructure
- [x] **Debug Logging**: Route logs over dedicated USB CDC-ACM 0 (virtual COM port) and configure USART6 (PC6 TX) as auxiliary hardware debug UART.
- [ ] **Power & USB-C CC Monitoring**: Read `cc1ADC` (PC5) and `cc2ADC` (PA7) via ADC1 to determine USB-C power budget (500mA, 1.5A, 3.0A) before enabling 3.3V power rail via `enable3V3` (PD6).
- [ ] **Power Status Indication**: Display error banner on LCD and set RGB status LED (PB8/PB9/PE6) if detected USB power is inadequate.
- [x] **Display**: SPI2 driver and `DisplayManager` for ST7789V status rendering ("BOOTING...", "WAIT CPLD", "READY").

## Phase 2: Host Connectivity & Dynamic CPLD Configuration (Path B)
- [ ] **USB High-Speed PHY (ULPI)**: Configure Zephyr `&usbotg_hs` with external USB3343 ULPI PHY for sustained 480 Mb/s throughput.
- [ ] **USB Device Composite Stack**:
  - Endpoint 0x81 Bulk IN: continuous 384 ksps I/Q sample stream (24.576 Mbps).
  - CDC-ACM 0: Interactive Zephyr Shell & serial console.
  - CDC-ACM 1: CBOR request/response control plane.
- [ ] **Firmware Version Handshake**: Report firmware version and protocol magic on connection; App verifies compatibility and displays an interactive modal if an update is required.
- [ ] **Dynamic CPLD Bitstream Loading**:
  - App streams ~7.1 KB iCE40 bitstream over USB (`CMD_LOAD_CPLD`).
  - MCU triggers Slave SPI configuration sequence over SPI3 simultaneously to both CPLDs (`cpld0NCS` PA4, `cpld1NCS` PA15, `cpldCRESET` PE1).
  - Monitor `cpldDONE` (PE0) pin for configuration success.
  - Validate register access by checking signature `0x4E785278` ("NxRx") on both CPLDs.

## Phase 3: Peripheral & Front-End Control
- [ ] **Direct GPIO Attenuator Array**: Implement `FrontEndDriver` controlling AS183-92LF step attenuator switches on GPIOD (`atten3dB` PD10, `atten6dB` PD7, `atten12dB` PD8, `atten24dB` PD9) for 0 to 45 dB attenuation in 3 dB steps.
- [ ] **Direct GPIO Filter Bank**: Implement BPF/HPF band selection on GPIOD (`bpf1` PD1, `bpf2` PD0, `bpf3` PD5, `bpf4` PD4, `hpf` PD2, `nhpf` PD3) to switch the 4 octave bandpass filters and broadcast AM high-pass filter.
- [ ] **Si5351 Synthesizer**: Complete I2C1 MultiSynth fractional PLL/divider calculation for OSD0 ($f - 12\text{kHz}$) and OSD1 ($f + 12\text{kHz}$), applying $8\times$ factor in 8-phase mode (< 15 MHz) and $4\times$ in 4-phase mode (> 15 MHz).
- [x] **Dual TLV320ADC5140 Audio ADCs**: Configure both quad-channel ADCs over SPI4 for 384 ksps 24-bit TDM/I2S output and integrated PGA gain control (0 to 42 dB).
- [ ] **GNSS Telemetry**: UART7 driver for GNSS receiver and 1pps TCXO discipline counter latch via CPLD registers.

## Phase 4: High-Speed Audio Capture & Cortex-M7 DSP Pipeline
- [ ] **SAI2 Capture Engine**:
  - Configure SAI2b (Master) on PA0/PA1/PA2/PA12 for OSD0 (4 channels, 24-bit, 384 ksps).
  - Configure SAI2a (Slave) on PD11 for OSD1 (4 channels, 24-bit, 384 ksps) synchronous to SAI2b clocks.
  - Set up circular ping-pong DMA buffers in AXI SRAM (`sram0` @ `0x24000000`).
- [ ] **On-Chip DSP Channel Reduction**:
  - Polyphase projection: Combine 4 differential phases per OSD into orthogonal $I/Q$ baseband pairs.
  - Complex phasor rotation: Rotate OSD0 by $-12\text{kHz}$ and OSD1 by $+12\text{kHz}$.
  - Coherent summation: $(OSD0_{\text{rot}} + OSD1_{\text{rot}}) \times 0.5$ for $+6\text{ dB}$ signal reinforcement and image rejection.
  - 4-Phase gain normalization: Automatically scale by $+3\text{ dB}$ ($\times \sqrt{2}$) when in 4-phase mode.
- [ ] **Fast Reflex AGC**: Peak detection $\max(|I|, |Q|)$ per frame; coordinate step attenuator switching with PGA gain reduction within $< 500\ \mu\text{s}$ to prevent ADC clipping.
- [ ] **Data Framing & USB Offload**: Pack interleaved 2-channel 384 ksps samples into fixed binary `IQPacketHeader` (Version 2) with timestamp and sequence numbering, submitting to USB Bulk IN endpoint.
