# NexRx: Implementation and Design Notes

This document captures non-obvious design decisions and architectural
details discovered during schematic analysis.

---

## 1. Active Bias Voltage Generation

### Design Choice
The receiver uses an **OPA1692** operational amplifier to buffer
resistive voltage dividers, creating the `+1.65V` (QSD) and `+2.5V`
(PGA) bias references.

### Rationale
*   **Low Source Impedance**: The QSD sampling capacitors (470pF)
    charge at frequencies up to 120 MHz. An active buffer ensures the
    bias point remains stable during these rapid switching events,
    preventing "bias sag" that would degrade I/Q balance.
*   **Noise Isolation**: The OPA1692 is a high-performance audio op-amp
    with extremely low noise. It filters residual noise from the
    digital power rails that might leak through the resistive
    dividers.
*   **Multiple Load Driving**: A single buffered reference can drive
    all three QSD channels and six PGA channels without crosstalk or
    voltage variation between channels.

---

## 2. RF Switching with pHEMTs

### Design Choice
The preselector and attenuator stages utilize **AS183-92LF** pHEMT
(pseudomorphic High Electron Mobility Transistor) switches.

### Rationale
*   **High Linearity**: With an Input IP3 of +43 dBm, these switches
    can handle very strong signals (up to +20 dBm) without
    introducing intermodulation distortion.
*   **Low Insertion Loss**: 0.3 dB loss at HF frequencies preserves
    system noise figure.
*   **Zero DC Power**: Unlike PIN diodes, pHEMT switches require
    negligible current to maintain their state, reducing heat and
    power consumption.

---

## 3. Cascaded Protection Strategy

The receiver implements a four-stage protection chain to ensure
survivability in harsh RF environments:

1.  **GDT (Gas Discharge Tube)**: Located at the SMA antenna input.
    Acts as the "heavy hitter" for lightning-induced surges or
    massive static discharge.
2.  **25V pk-pk Limiter**: A TVS diode array before the digital
    attenuators. Protects the attenuator switch chips from nearby
    high-power transmitters (+40 dBm survival).
3.  **13V pk-pk Limiter**: A second stage before the preselector.
    Protects the sensitive QSD analog switches from transients that
    bypass the primary limiter.
4.  **ADC Input Diodes**: Final BAV99-style clamping at the AK5578
    inputs to ensure signals never exceed the ADC's power rails.

---

## 4. Transformer Symmetry

### Design Choice
The 200Ω to 3x22Ω output transformer uses **hexafilar winding** on a
BN-43-202 binocular core.

### Rationale
*   **Phase/Amplitude Matching**: The triple-QSD architecture relies
    on mathematical cancellation of harmonics. This requires the
    three RF inputs to the QSDs to be as identical as possible.
*   **Magnetic Coupling**: Hexafilar winding (twisting all six wires
    together before winding) ensures that leakage inductance and
    coupling coefficients are perfectly matched across all channels.

## 5. QSD Biasing Implementation

### Design Choice
The final implementation uses **10kΩ** bias resistors for the QSD inputs (instead of the 100kΩ originally considered).

### Rationale
*   **Thermal Noise Reduction**: Lower resistance values reduce the
    Johnson-Nyquist noise contribution at the sensitive QSD input stage.
*   **Improved Settling Time**: 10kΩ provides a faster RC time constant
    with the AC coupling capacitors, ensuring the DC bias point settles
    quickly during power-up or rapid signal transients.
*   **Dynamic Range**: The reduced noise floor directly contributes to
    the high dynamic range targets of the receiver.

---

## 6. High-Speed USB Connectivity

### Design Choice
The receiver utilizes an external **USB3343** ULPI (UTMI+ Low Pin Interface)
transceiver instead of the STM32's internal Full-Speed PHY.

### Rationale
*   **High-Speed Data Rates**: The NexRx streams six channels of 24-bit,
    96 kHz I/Q data. At 15-20 Mbps of raw throughput (plus overhead),
    this exceeds the reliable capacity of standard 12 Mbps Full-Speed USB.
*   **480 Mbps Capability**: The USB3343 provides a true High-Speed (480 Mbps)
    interface, ensuring that the USB bus is never a bottleneck for
    real-time spectral visualization and audio processing.
*   **Offloading Microcontroller**: Using an external PHY allows the
    STM32H753 to focus its computational resources on DSP and AGC
    tasks rather than managing the low-level physical USB signaling.
