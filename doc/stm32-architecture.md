# STM32H743VIT6 Architecture & Peripheral Allocation

This document details the hardware allocation for the
**STM32H743VIT6** MCU in the NexRx project.

## Peripheral Allocation Summary

### SPI Controllers
* **SPI2**: ST7789V Display Interface (`dispNSS` PA11, `dispSCK` PA9, `dispMOSI` PC1, `dispDC` PE13, `dispRESET` PE12)
* **SPI3**: Dual CPLD Interface (`cpld0NCS` PA4, `cpld1NCS` PA15, `cpldSCK` PC10, `cpldMISO` PC11, `cpldMOSI` PB2, `cpldDONE` PE0, `cpldCRESET` PE1)
* **SPI4**: Dual TLV320ADC5140 ADC Control Bus (`osd0NSS` PE3, `osd1NSS` PE4, `osdSCK` PE2, `osdMOSI` PE14, `osdMISO` PE5, `adcReset` PA6)

### I2C Controllers
* **I2C1**: Si5351 Synthesizer Configuration & Status (`si5351SCL` PB6, `si5351SDA` PB7, `si5351OE` PB4)

### USART / UART Mapping
* **UART7**: GNSS Receiver Interface (`gnssRX` PE8, `gnssTX` PE9, `gnssNRESET` PB3)

### SAI (Serial Audio Interface) Mapping
* **SAI2a (Master)**: OSD0 audio serial link (`osd0` PA0 data, `osdMCLK` PA1, `osdBCLK` PA2, `osdWCLK` PA12)
* **SAI2b (Slave)**: OSD1 audio serial link (`osd1` PD11 data; synchronous to SAI2a clocks)

Each TLV320ADC5140 ADC provides four 24-bit samples at 384ksps per OSD over its serial audio link to the STM32. The STM32 performs on-chip DSP to combine and correlate these into two 384ksps streams (I and Q) sent over USB Bulk (`iq_data_pump`) to the host application.

### ADC Channels
* **ADC1**: USB Type-C CC pin monitoring (`cc1ADC` PC5, `cc2ADC` PA7) for power source validation.

### USB Interface
* **USB_OTG_HS**: High-Speed ULPI interface (480 Mbps) to external USB3343 transceiver.

---

## Hardware Pin Allocation Table

| Symbol | Peripheral | Pin | Type | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `osd0` | SAI2a | PA0 | input | Master audio data (OSD0) |
| `osdMCLK` | SAI2a | PA1 | output | Master clock |
| `osdBCLK` | SAI2a | PA2 | output | Bit clock |
| `osdWCLK` | SAI2a | PA12 | output | Word clock (Frame Sync) |
| `osd1` | SAI2b | PD11 | input | Slave audio data (OSD1) |
| `dispNSS` | SPI2 | PA11 | output | Display chip select |
| `dispSCK` | SPI2 | PA9 | output | Display SPI clock |
| `dispMOSI` | SPI2 | PC1 | output | Display SPI MOSI |
| `dispDC` | GPIO | PE13 | output | Display data/command select |
| `dispRESET` | GPIO | PE12 | output | Display reset |
| `cpld0NCS` | SPI3 | PA4 | output | CPLD0 SPI chip select |
| `cpld1NCS` | SPI3 | PA15 | output | CPLD1 SPI chip select |
| `cpldSCK` | SPI3 | PC10 | output | CPLD SPI clock |
| `cpldMISO` | SPI3 | PC11 | input | CPLD SPI MISO |
| `cpldMOSI` | SPI3 | PB2 | output | CPLD SPI MOSI |
| `cpldDONE` | GPIO | PE0 | input | CPLD configuration done |
| `cpldCRESET` | GPIO | PE1 | output | CPLD configuration reset |
| `osd0NSS` | SPI4 | PE3 | output | OSD0 ADC (TLV320ADC5140) SPI select |
| `osd1NSS` | SPI4 | PE4 | output | OSD1 ADC (TLV320ADC5140) SPI select |
| `osdSCK` | SPI4 | PE2 | output | OSD ADC SPI clock |
| `osdMOSI` | SPI4 | PE14 | output | OSD ADC SPI MOSI |
| `osdMISO` | SPI4 | PE5 | input | OSD ADC SPI MISO |
| `adcReset` | GPIO | PA6 | output | Reset line for ADCs |
| `bpf2` | GPIO | PD0 | output | Filter selection (BPF branch 2) |
| `bpf1` | GPIO | PD1 | output | Filter selection (BPF branch 1) |
| `hpf` | GPIO | PD2 | output | Filter selection (HPF enable) |
| `nHPF` | GPIO | PD3 | output | Inverted HPF selection |
| `bpf4` | GPIO | PD4 | output | Filter selection (BPF branch 4) |
| `bpf3` | GPIO | PD5 | output | Filter selection (BPF branch 3) |
| `enable3V3` | GPIO | PD6 | output | 3.3V power rail enable |
| `atten6dB` | GPIO | PD7 | output | 6 dB attenuator stage |
| `atten12dB` | GPIO | PD8 | output | 12 dB attenuator stage |
| `atten24dB` | GPIO | PD9 | output | 24 dB attenuator stage |
| `atten3dB` | GPIO | PD10 | output | 3 dB attenuator stage |
| `cc1ADC` | ADC1 | PC5 | input | USB CC1 voltage |
| `cc2ADC` | ADC1 | PA7 | input | USB CC2 voltage |
| `statusRed` | GPIO | PB9 | output | RGB Status LED - Red (active low) |
| `statusGreen` | GPIO | PB8 | output | RGB Status LED - Green (active low) |
| `statusBlue` | GPIO | PE6 | output | RGB Status LED - Blue (active low) |
| `gnssNRESET` | GPIO | PB3 | output | GNSS reset (active low) |
| `gnssRX` | UART7 | PE8 | input | GNSS serial receive |
| `gnssTX` | UART7 | PE9 | output | GNSS serial transmit |
| `si5351OE` | GPIO | PB4 | output | Si5351 output enable |
| `si5351SCL` | I2C1 | PB6 | bidir | Si5351 I2C clock |
| `si5351SDA` | I2C1 | PB7 | bidir | Si5351 I2C data |
| `swdSWDIO` | SWD | PA13 | bidir | SWD debug data |
| `swdSWDCLK` | SWD | PA14 | input | SWD debug clock |
| `usbDATA0` | USB_HS | PA3 | bidir | USB ULPI Data 0 |
| `usbCLKOUT` | USB_HS | PA5 | input | USB ULPI Clock |
| `usbDATA1` | USB_HS | PB0 | bidir | USB ULPI Data 1 |
| `usbDATA2` | USB_HS | PB1 | bidir | USB ULPI Data 2 |
| `usbDATA7` | USB_HS | PB5 | bidir | USB ULPI Data 7 |
| `usbDATA3` | USB_HS | PB10 | bidir | USB ULPI Data 3 |
| `usbDATA4` | USB_HS | PB11 | bidir | USB ULPI Data 4 |
| `usbDATA5` | USB_HS | PB12 | bidir | USB ULPI Data 5 |
| `usbDATA6` | USB_HS | PB13 | bidir | USB ULPI Data 6 |
| `usbSTP` | USB_HS | PC0 | output | USB ULPI Stop |
| `usbDIR` | USB_HS | PC2_C | input | USB ULPI Direction |
| `usbNXT` | USB_HS | PC3_C | input | USB ULPI Next |
| `STM32Clock` | RCC | PH0 | input | 26MHz Crystal Resonator (HSE) |

---

## Firmware Integration (Zephyr RTOS)

The embedded firmware resides in the `fw/` directory.

### Subsystem Driver Allocation:
1. **SPI (`zephyr,spi-stm32`)**:
   - SPI2: ST7789V display driver.
   - SPI3: Dual CPLD register and configuration interface.
   - SPI4: Dual TLV320ADC5140 ADC SPI control driver.
2. **I2C (`zephyr,i2c-stm32`)**: Controls Si5351 synthesizer tuning (I2C1).
3. **UART (`zephyr,uart-stm32`)**: Handles GNSS NMEA/UBX telemetry parsing (UART7).
4. **SAI (`zephyr,i2s-stm32`)**: Captures four 24-bit samples at 384ksps per OSD across SAI2a (master) and SAI2b (slave) with DMA buffering.
5. **USB (`zephyr,usb-device`)**: High-speed USB 2.0 offload (480 Mbps) using the external ULPI PHY.
6. **ADC (`zephyr,adc-stm32`)**: Monitors CC1 and CC2 pin voltages to validate Type-C power negotiation before enabling 3.3V analog supplies.
7. **GPIO (`zephyr,gpio-stm32`)**:
   - BPF/HPF filter bank selection (`bpf1`..`bpf4`, `hpf`, `nHPF`).
   - Attenuator stage switching (`atten3dB`, `atten6dB`, `atten12dB`, `atten24dB`).
   - Power rail control (`enable3V3`).
   - Status RGB LEDs (`statusRed`, `statusGreen`, `statusBlue`).
   - Hardware reset control (`adcReset`, `gnssNRESET`, `cpldCRESET`).

