# STM32H743VIT6 Architecture & Peripheral Allocation

This document details the hardware allocation for the
**STM32H743VIT6** MCU in the NexRx project.

## Peripheral Allocation Summary

### SPI Controllers
* **SPI2**: ST7789V Display Interface (`disp`)
* **SPI3**: Dual CPLD Interface (`CPLD0`, `CPLD1`)
* **SPI4**: Analog Front-End / PGA Control (`pga`)

### I2C Controllers
* **I2C1**: Si5351 Synthesizer Configuration & Status (`si5351`)
* **I2C4**: Audio Codec Configuration (`audioCodec`)

### USART / UART Mapping
* **UART7**: GNSS Receiver Interface (RX = PE8, TX = PE9)
* **USART6**: Debug Console / Tuning Output (`usart6`)

### SAI (Serial Audio Interface) Mapping
* **SAI2**: Sync Slave (OSD0 / OSD1 Audio Data Channels)
* **SAI3**: Master + Clock (Audio Channel 1 + BCLK / WCLK / MCLK)
* **SAI4**: Sync Slave (Audio Channel 4)

| SAI Channel | Function | Pin | Label | Description |
|-------------|----------|-----|-------|-------------|
| SAI3_A | Master Data 1 | PD1 | `Ctrl.osdData1` | OSD0 Channel 1 Data |
| SAI2_B | Slave Data 2 | PA0 | `Ctrl.osdData2` | OSD0 Channel 2 Data |
| SAI2_A | Slave Data 3 | PD11 | `Ctrl.osdData3` | OSD1 Channel 1 Data |
| SAI4_A | Slave Data 4 | PE6 | `Ctrl.osdData4` | OSD1 Channel 2 Data |
| SAI3_SCK_A | Bit Clock | PD0 | `Ctrl.osdBCLK` | Codec Audio Bit Clock |
| SAI3_FS_A | Word Clock | PD4 | `Ctrl.osdWCLK` | Codec Audio Frame Sync (LRCLK) |
| SAI3_MCLK_A | Master Clock | PD15 | `Ctrl.osdMCLK` | Codec Audio Master Clock |

## Pin Allocation Table

| Pin | Function | Label | Description |
|-----|----------|-------|-------------|
| PH0 | RCC_OSC_IN | `STM32Clock` | 26MHz Crystal Resonator (HSE) |
| PA0 | SAI2_SD_B | `Ctrl.osdData2` | OSD Audio Data Channel 2 |
| PA3 | USB_OTG_HS_ULPI_D0 | `usbDATA0` | USB High-Speed PHY Data 0 |
| PA4 | SPI3_NSS | `CPLD.NSS` | Dual CPLD SPI Chip Select |
| PA5 | USB_OTG_HS_ULPI_CK | `usbCLKOUT` | USB High-Speed PHY Clock |
| PA6 | GPIO_Output | `Ctrl.audioReset` | Audio Codec Reset Line |
| PA7 | ADC1_INP7 | `Ctrl.usbCC2adc` | USB-C CC2 Voltage Monitoring |
| PA9 | SPI2_SCK | `dispSCK` | Display SPI Clock |
| PA11 | SPI2_NSS | `dispNSS` | Display SPI Chip Select |
| PA13 | SWDIO | `SWDIO` | SWD Debug Data |
| PA14 | SWCLK | `SWCLK` | SWD Debug Clock |
| PB0 | USB_OTG_HS_ULPI_D1 | `usbDATA1` | USB High-Speed PHY Data 1 |
| PB1 | USB_OTG_HS_ULPI_D2 | `usbDATA2` | USB High-Speed PHY Data 2 |
| PB2 | SPI3_MOSI | `CPLD.MOSI` | CPLD SPI MOSI |
| PB5 | USB_OTG_HS_ULPI_D7 | `usbDATA7` | USB High-Speed PHY Data 7 |
| PB10 | USB_OTG_HS_ULPI_D3 | `usbDATA3` | USB High-Speed PHY Data 3 |
| PB11 | USB_OTG_HS_ULPI_D4 | `usbDATA4` | USB High-Speed PHY Data 4 |
| PB12 | USB_OTG_HS_ULPI_D5 | `usbDATA5` | USB High-Speed PHY Data 5 |
| PB13 | USB_OTG_HS_ULPI_D6 | `usbDATA6` | USB High-Speed PHY Data 6 |
| PC0 | USB_OTG_HS_ULPI_STP | `usbSTP` | USB High-Speed PHY Stop |
| PC1 | SPI2_MOSI | `dispMOSI` | Display SPI MOSI |
| PC2_C | USB_OTG_HS_ULPI_DIR | `usbDIR` | USB High-Speed PHY Direction |
| PC3_C | USB_OTG_HS_ULPI_NXT | `usbNXT` | USB High-Speed PHY Next |
| PC5 | ADC1_INP8 | `Ctrl.usbCC1adc` | USB-C CC1 Voltage Monitoring |
| PC6 | USART6_TX | `Tune.txrx` | Tuning Serial Interface / Debug |
| PC10 | SPI3_SCK | `CPLD.SCK` | CPLD SPI Clock |
| PC11 | SPI3_MISO | `CPLD.MISO` | CPLD SPI MISO |
| PD0 | SAI3_SCK_A | `Ctrl.osdBCLK` | Codec Bit Clock |
| PD1 | SAI3_SD_A | `Ctrl.osdData1` | OSD Audio Data Channel 1 |
| PD4 | SAI3_FS_A | `Ctrl.osdWCLK` | Codec Word Clock (LRCLK) |
| PD11 | SAI2_SD_A | `Ctrl.osdData3` | OSD Audio Data Channel 3 |
| PD12 | I2C4_SCL | `Ctrl.audioSCL` | Audio Control I2C SCL |
| PD13 | I2C4_SDA | `Ctrl.audioSDA` | Audio Control I2C SDA |
| PD15 | SAI3_MCLK_A | `Ctrl.osdMCLK` | Codec Master Clock |
| PE6 | SAI4_SD_A | `Ctrl.osdData4` | OSD Audio Data Channel 4 |
| PE8 | UART7_RX | `gnssRX` | GNSS Receiver Serial RX |
| PE9 | UART7_TX | `gnssTX` | GNSS Receiver Serial TX |
| PE12 | GPIO_Output | `dispRESET` | Display Reset Line |
| PE13 | GPIO_Output | `dispDC` | Display Data/Command Select |
| PE14 | SPI4_MOSI | `Ctrl.pgaMOSI` | PGA SPI MOSI |

## Firmware Integration (Zephyr RTOS v4.1)

The embedded firmware resides in the `fw/` directory.

### Subsystem Driver Allocation:
1. **SPI (`zephyr,spi-stm32`)**: Manages CPLD register configuration
   and ST7789V display driving.
2. **I2C (`zephyr,i2c-stm32`)**: Controls Si5351 synthesizer tuning
   (I2C1) and audio codec configuration (I2C4).
3. **UART (`zephyr,uart-stm32`)**: Handles GNSS NMEA/UBX telemetry
   parsing (UART7) and shell/logging output (USART6).
4. **I2S / SAI (`zephyr,i2s-stm32`)**: Captures 24-bit audio-rate I/Q
   streams from the audio codecs with DMA buffering.
5. **USB (`zephyr,usb-device`)**: High-speed USB 2.0 offload (480
   Mbps) using the external ULPI PHY.
6. **GPIO (`zephyr,gpio-stm32`)**: Direct drive control for BPF band
   selection relays, attenuator pads, and hardware status lines.
   Hardware power sequencing is automatic on power-up.

