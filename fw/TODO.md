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

## Phase 2: Host Connectivity (USB)
...
## Phase 4: Peripheral Drivers & Control
- [ ] **NexBus (GPIO Expanders)**: Implement the custom 1 Mb/s pulse-width
  protocol over USART6 (`Tune.txrx` pin) in half-duplex mode to control remote
  hardware. 

  - [ ] Implement Mode A (Pulse-Width) using TIM3+DMA.
  - [ ] Implement Mode B (UART 115.2k) for firmware updates.
  - [ ] Implement C011 firmware validation and management logic (checking
    versions and pushing updates).
  - [ ] **Silence-to-Reset**: Implement IWDG on C011 firmware (`fw-expander`) 
    that triggers a reset if no NexBus activity is detected, allowing 
    recovery without dedicated reset lines.

## Phase 2: Host Connectivity (USB)
- [ ] **USB High-Speed PHY (ULPI)**: Configure Zephyr to use the external USB3343 ULPI PHY for 480Mb/s operation.
- [ ] **USB Device Stack**: Set up endpoints. We likely need:
  - CDC-ACM (Control/Logging)
  - Bulk IN (High-bandwidth I/Q streaming to Host)
  - Bulk OUT (Future: Transmit audio/data from Host)
- [ ] **Control Protocol**: Implement the CBOR-based request/response protocol (mirroring the Twin's TCP behavior) over the USB control interface.

## Phase 3: FPGA Management
- [ ] **Bitstream Loading**: Implement iCE40 Slave SPI configuration sequence
  via SPI3.
  - [ ] Store default bitstream in MCU internal flash as a `const uint8_t` 
    array.
  - [ ] Implement GPIO sequence (PROG/NSS) to trigger Slave SPI mode.
  - [ ] Send bitstream + dummy clocks.
  - [ ] Monitor `DONE` pin for success.

- [ ] **Bitstream Update**: Implement a CBOR control command to receive a 
  new bitstream from the host and write it to a dedicated flash partition.

- [ ] **FPGA Reset & Lifecycle**: Manage the FPGA reset state and ensure it
  is ready before communicating.

## Phase 4: Peripheral Drivers & Control
- [ ] **NexBus (GPIO Expanders)**: Implement the custom 1 Mb/s pulse-width protocol over USART6 (`Tune.txrx` pin) in half-duplex mode to control remote hardware.
- [ ] **AK5578 Audio Codec**: I2C4 driver to initialize the codec, set sample rates, and configure the TDM/multi-line output.
- [ ] **MAX9939 PGAs**: SPI4 stubs to configure the gain for the QSD baseband stages.
- [ ] **Display**: SPI2 driver and stubs for basic ST7789V graphics rendering.

## Phase 5: High-Speed Data Path
- [ ] **SAI / I2S Engine**: Configure SAI2, SAI3, and SAI4 to capture the
  multi-channel synchronous data from the AK5578.

- [ ] **DMA & Buffering**: Set up double-buffered DMA to pull SAI data into
  SRAM without CPU intervention.
  - [ ] **AXI SRAM Placement**: Explicitly place high-bandwidth buffers in the
    `.axi_sram` section to utilize the D1 domain's performance.

- [ ] **Stream Offload**: Feed the DMA buffers into the USB Bulk IN endpoint.
  - [ ] **Data Plane Optimization**: Use a fixed binary header instead of CBOR
    for the I/Q stream to reduce MCU overhead.
  - [ ] **Hardware Interleaving**: Use MDMA (Master DMA) to interleave the
    multi-lane SAI buffers into a single contiguous stream for USB offload.
  - [ ] **Data Loss Detection**: Implement sequence numbering and MCU overrun 
    tracking in the binary header.
  - [ ] **Simulation & Testing**: Use Zephyr 'native_sim' to validate the pump 
    logic and interleaving patterns on the host before hardware deployment.
