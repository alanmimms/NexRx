# STM32H753VITx Architecture & Pin Mapping

This document details the hardware allocation for the STM32H753VITx MCU in the NexRx project.

## Peripheral Allocation Summary

### SPI Controllers
Based on schematic text blocks:
* **SPI1**: Not Used
* **SPI2**: Display (`disp`)
* **SPI3**: FPGA Interface (`FPGA`)
* **SPI4**: QSD PGA Control (`pga`)
* **SPI5**: Not Used
* **SPI6**: Not Used

### SAI (Serial Audio Interface) Mapping
Based on schematic text blocks and pin labels:
* **SAI1**: Not Used
* **SAI2**: Sync Slave (Data 2 & 3)
* **SAI3**: Master + Clock (Data 1 + BCLK/WCLK/MCLK)
* **SAI4**: Sync Slave (Data 4)

| SAI Channel | Function | Pin | Label |
|-------------|----------|-----|-------|
| SAI3_A | Master Data 1 | PD1 | `Ctrl.qsdData1` |
| SAI2_B | Slave Data 2 | PA0 | `Ctrl.qsdData2` |
| SAI2_A | Slave Data 3 | PD11 | `Ctrl.qsdData3` |
| SAI4_A | Slave Data 4 | PE6 | `Ctrl.qsdData4` |
| SAI3_SCK_A | Bit Clock | PD0 | `Ctrl.qsdBCLK` |
| SAI3_FS_A | Word Clock | PD4 | `Ctrl.qsdWCLK` |
| SAI3_MCLK_A | Master Clock | PD15 | `Ctrl.qsdMCLK` |

## Pin Table

| Pin | Function | Label | Description |
|-----|----------|-------|-------------|
| PH0 | RCC_OSC_IN | `STM32Clock` | 26MHz Crystal Resonator (HSE) |
| PA0 | SAI2_SD_B | `Ctrl.qsdData2` | QSD Data Channel 2 |
| PA3 | USB_OTG_HS_ULPI_D0 | `usbDATA0` | USB High-Speed PHY Data 0 |
| PA4 | SPI3_NSS | `FPGA.NSS` | FPGA SPI Chip Select |
| PA5 | USB_OTG_HS_ULPI_CK | `usbCLKOUT` | USB High-Speed PHY Clock |
| PA6 | GPIO_Output | `Ctrl.audioReset` | Audio Codec Reset |
| PA7 | ADC1_INP7 | `Ctrl.usbCC2adc` | USB-C CC2 Voltage Monitoring |
| PA9 | SPI2_SCK | `dispSCK` | Display SPI Clock |
| PA11 | SPI2_NSS | `dispNSS` | Display SPI Chip Select |
| PA13 | SWDIO | `SWDIO` | Debug Interface |
| PA14 | SWCLK | `SWCL` | Debug Interface |
| PB0 | USB_OTG_HS_ULPI_D1 | `usbDATA1` | USB High-Speed PHY Data 1 |
| PB1 | USB_OTG_HS_ULPI_D2 | `usbDATA2` | USB High-Speed PHY Data 2 |
| PB2 | SPI3_MOSI | `FPGA.MOSI` | FPGA SPI MOSI |
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
| PC6 | USART6_TX | `Tune.txrx` | Tuning Serial Interface |
| PC10 | SPI3_SCK | `FPGA.SCK` | FPGA SPI Clock |
| PC11 | SPI3_MISO | `FPGA.MISO` | FPGA SPI MISO |
| PD0 | SAI3_SCK_A | `Ctrl.qsdBCLK` | QSD Bit Clock |
| PD1 | SAI3_SD_A | `Ctrl.qsdData1` | QSD Data Channel 1 |
| PD4 | SAI3_FS_A | `Ctrl.qsdWCLK` | QSD Word Clock (LRCLK) |
| PD6 | GPIO_Output | `Ctrl.enable3V3` | 3.3V Power Enable |
| PD7 | GPIO_Output | `Ctrl.enable3VA` | 3.3V Analog Power Enable |
| PD8 | GPIO_Output | `Ctrl.enable5VA` | 5V Analog Power Enable |
| PD9 | GPIO_Output | `Ctrl.enableFPGAVCORE` | FPGA Vcore Enable |
| PD11 | SAI2_SD_A | `Ctrl.qsdData3` | QSD Data Channel 3 |
| PD12 | I2C4_SCL | `Ctrl.audioSCL` | Audio Control I2C SCL |
| PD13 | I2C4_SDA | `Ctrl.audioSDA` | Audio Control I2C SDA |
| PD15 | SAI3_MCLK_A | `Ctrl.qsdMCLK` | QSD Master Clock |
| PE0 | GPIO_Input | `FPGA.DONE` | FPGA Configuration Done |
| PE1 | GPIO_Output | `FPGA.PROG` | FPGA Program Trigger |
| PE2 | SPI4_SCK | `Ctrl.pgaSCK` | QSD PGA SPI Clock |
| PE4 | SPI4_NSS | `Ctrl.pgaNSS` | QSD PGA SPI Chip Select |
| PE6 | SAI4_SD_A | `Ctrl.qsdData4` | QSD Data Channel 4 |
| PE12 | GPIO_Output | `dispRESET` | Display Reset |
| PE13 | GPIO_Output | `dispDC` | Display Data/Command Select |
| PE14 | SPI4_MOSI | `Ctrl.pgaMOSI` | QSD PGA SPI MOSI |

## Firmware Integration (Zephyr OS)

The firmware will be located in the `fw/` directory.

### Key Drivers/Subsystems:
1. **SPI (zephyr,spi-stm32)**: For FPGA control and Display communication.
2. **I2S/SAI (zephyr,i2s-stm32)**: Crucial for high-speed QSD data capture. Since the STM32H7 SAI blocks can operate in synchronized mode, one block (SAI3) will provide the clocking for the others (SAI2, SAI4) to ensure phase-coherent sampling.
3. **USB (zephyr,usb-device)**: High-speed data offload via the external ULPI PHY.
4. **GPIO (zephyr,gpio-stm32)**: Power sequencing and FPGA configuration management.

### FPGA-STM32 Interaction:
* **Configuration**: STM32 manages FPGA bitstream loading via `FPGA.PROG` and monitors `FPGA.DONE`.
* **Control**: SPI3 serves as the primary control bus for the FPGA logic.
* **Data Path**: While the QSD data comes via SAI, processed RF or control data may be exchanged via SPI3.
