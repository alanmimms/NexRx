# NexRx Firmware (fw) - Development TODO

## Phase 1: Basic Bring-Up & Infrastructure
- [x] **Debug Logging**: Set up Zephyr's logging subsystem. Decide whether to
  route logs over a dedicated USB CDC-ACM (virtual COM port) or a physical
  UART for early bring-up before USB is stable.

- [ ] **Power Sequencing**: Implement a sequence to safely bring up the power
  rails (FPGA VCORE -> 100ms -> 3.3V Digital -> 100ms -> 3.3V Analog -> 100ms
  -> 5V Analog) via GPIOs. Log each stage's success and timing.

- [ ] **ADC Monitoring**: Read `Ctrl.usbCC1adc` and `Ctrl.usbCC2adc` to
  determine USB-C current limits before fully enabling high-power rails. Log
  the detected capacity (e.g., 500mA, 1.5A, 3.0A).

- [ ] **Power Status Indication**: Implement logic to blink an LED or display
  an error message if the detected USB power is inadequate for full operation
  (< 15W/3A).

- [ ] **LittleFS File System**: Implement a littleFS based file system
      in a region of MCU flash memory to contain firmware files for
      STM32C011 GPIO expanders (NexBus), the FPGA config image, and
      the calibration data for the hardware.

## Phase 2: Host Connectivity (USB)
- [ ] **USB High-Speed PHY (ULPI)**: Configure Zephyr to use the external 
  USB3343 ULPI PHY for 480Mb/s operation.

- [ ] **USB Device Stack (v3)**: Finalize the composite device with two 
  CDC-ACM ports and the custom Vendor Bulk interface.

- [ ] **Control Protocol**: Implement the CBOR-based request/response protocol 
  (mirroring the Twin's TCP behavior) over the second USB serial interface.

## Phase 3: FPGA Management
- [x] **Bitstream Loading**: Implement iCE40 Slave SPI configuration sequence
  via SPI3.
  - [x] Implement GPIO sequence (PROG/NSS) to trigger Slave SPI mode.
  - [x] Send bitstream + dummy clocks.
  - [x] Monitor `DONE` pin for success.
  - [x] Validate SPI path with register write/read.

- [ ] **Bitstream Persistence**: Store default bitstream in MCU internal flash 
  as a `const uint8_t` array or in a dedicated flash partition.

- [ ] **Bitstream Update**: Implement a CBOR control command to receive a 
  new bitstream from the host and write it to flash.

## Phase 4: Peripheral Drivers & Control
- [ ] **NexBus (GPIO Expanders)**: Implement the custom 1 Mb/s pulse-width
  protocol over USART6 (`Tune.txrx` pin) in half-duplex mode.
  - [x] Implement Mode A (Pulse-Width) using TIM3+DMA.
  - [x] Implement Mode B (UART 115.2k) for firmware updates.
  - [ ] Implement C011 firmware validation and management logic.
  - [x] **Silence-to-Reset**: Implement IWDG on C011 firmware (`fw-expander`) 
    for recovery without dedicated reset lines.

- [x] **AK5578 Audio Codec**: I2C4 driver to initialize the codec and 
  configure for 4-lane I2S output.

- [x] **MAX9939 PGAs**: SPI4 driver to configure the gain for the QSD stages.

- [x] **Display**: SPI2 driver and `DisplayManager` for status rendering.

## Phase 5: High-Speed Data Path
- [ ] **SAI Capture Engine**: Finalize capture from SAI2, SAI3, and SAI4.
  - [ ] **DMA & Buffering**: Set up double-buffered DMA to pull SAI data into 
    AXI SRAM buffers.
  - [ ] **Data Plane Optimization**: Use a fixed binary header (Version 2) 
    instead of CBOR for the I/Q stream.
  - [ ] **Hardware Interleaving**: Implement MDMA-based gather-scatter to 
    interleave lane data for USB offload.

- [ ] **Data Loss Detection**: Implement sequence numbering and MCU overrun 
  tracking in the binary header.

- [ ] **Simulation & Testing**: Use Zephyr 'native_sim' to validate the pump 
  logic on the host.

## Phase 6: Signal Processing & AGC
- [ ] **AGC Manager**: Implement high-priority gain control in the MCU.
  - [ ] **Reflexes**: Fast-attack response to clipping detected by AK5578 or 
    peak analysis (< 500us reaction).
  - [ ] **Gain Coordination**: Synchronize NexBus attenuator switches with PGA 
    gain steps to eliminate audible jumps.
  - [ ] **Policy API**: Interface for the Host to set AGC modes (Fast, Slow, 
    etc.) and parameters via CBOR.

- [ ] **Twin Enhancement**: Update the Digital Twin to support varying 
  signal strengths and fading models to validate AGC behavior.
