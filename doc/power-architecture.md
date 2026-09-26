# NexRx Power Architecture

The NexRx receiver is powered from the USB VBUS supply (5V at 3A expected). This provides up to 15W of power, which is significantly more than required for the dual-OSD architecture, dual iCE40 CPLDs, and supporting digital logic.

## Power Rail Summary

| Rail | Voltage | Capacity | Control | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **VBUS** | 5.0V | 3.0A | Always On | Primary input power from USB-C. |
| **+3.3V** | 3.3V | 3.0A | Automatic | Main digital logic supply (MCU, CPLDs, Si5351). |
| **+5VA** | 5.0V | 300mA | Automatic | Analog supply for audio ADCs / Codecs. |
| **+3.3VA** | 3.3V | 300mA | Automatic | Clean analog supply for RF front-end, PGAs, and buffers. |

## Design Verification & Power Sequencing Notes

1.  **Automatic Power Sequencing:** Subsystem power rails feature hardware RC delay networks and automatic LDO/regulator sequencing. MCU software control is not required to sequence power rails during startup or shutdown.
2.  **Input Protection:** The VBUS input includes ESD protection and a TVS diode to clamp input surges.
3.  **Conversion Efficiency:** The main 3.3V digital rail uses a high-efficiency synchronous buck converter to minimize thermal dissipation inside the enclosure.
4.  **Analog Integrity:** Sensitive analog rails (+5VA, +3.3VA) utilize ultra-high PSRR LDO regulators to preserve low noise floor and high dynamic range.

