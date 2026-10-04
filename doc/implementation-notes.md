# NexRx: Implementation and Design Notes

This document captures non-obvious design decisions and architectural
details discovered during schematic analysis.

---

## Active Bias Voltage Generation

### Design Choice
The receiver uses an **OPA1692** operational amplifier to buffer
resistive voltage dividers, creating the `+1.65V` (OSD) reference.

### Rationale
*   **Low Source Impedance**: The OSD sampling capacitors (8.2nF)
    charge at frequencies up to 120 MHz. An active buffer ensures the
    bias point remains stable during these rapid switching events,
    preventing "bias sag" that would degrade I/Q balance.
*   **Noise Isolation**: The OPA1692 is a high-performance audio op-amp
    with extremely low noise. It filters residual noise from the
    digital power rails that might leak through the resistive
    dividers.
*   **Multiple Load Driving**: A single buffered reference can drive
    both OSD channels crosstalk or voltage variation between channels.

---

## RF Switching with pHEMTs

### Design Choice
The preselector and attenuator stages utilize **AS183-92LF** -- and
their relatives, **SKY13322-375LF** -- pHEMT (pseudomorphic High
Electron Mobility Transistor) switches.

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

## Cascaded Protection Strategy

The receiver implements a four-stage protection chain to ensure
survivability in harsh RF environments:

1.  **GDT (Gas Discharge Tube)**: Located at the SMA antenna input.
    Acts as the "heavy hitter" for lightning-induced surges or
    massive static discharge.
2.  **25V pk-pk Limiter**: A TVS diode array before the digital
    attenuators. Protects the attenuator switch chips from nearby
    high-power transmitters (+40 dBm survival).
3.  **13V pk-pk Limiter**: A second stage before the preselector.
    Protects the sensitive OSD analog switches from transients that
    bypass the primary limiter.
4.  **ADC Input Diodes**: Final BAV99-style clamping at the TLV320ADC5140
    inputs to ensure signals never exceed the ADC's power rails.

---

## Transformer Symmetry

### Design Choice
The 200Ω to 3x22Ω output transformer uses **hexafilar winding** on a
BN-43-202 binocular core.

### Rationale
*   **Phase/Amplitude Matching**: The dual-OSD architecture relies
    on mathematical cancellation of images and harmonics. This requires the
    two RF inputs to the OSDs to be as identical as possible.
*   **Magnetic Coupling**: Hexafilar winding (twisting all six wires
    together before winding) ensures that leakage inductance and
    coupling coefficients are perfectly matched across all channels.

## OSD Biasing Implementation

### Design Choice
The final implementation uses **10kΩ** bias resistors for the OSD inputs (instead of the 100kΩ originally considered).

### Rationale
*   **Thermal Noise Reduction**: Lower resistance values reduce the
    Johnson-Nyquist noise contribution at the sensitive OSD input stage.
*   **Improved Settling Time**: 10kΩ provides a faster RC time constant
    with the AC coupling capacitors, ensuring the DC bias point settles
    quickly during power-up or rapid signal transients.
*   **Dynamic Range**: The reduced noise floor directly contributes to
    the high dynamic range targets of the receiver.

---

## High-Speed USB Connectivity

### Design Choice
The receiver utilizes an external **USB3343** ULPI (UTMI+ Low Pin Interface)
transceiver instead of the STM32's internal Full-Speed PHY.

### Rationale
*   **High-Speed Data Rates**: The NexRx streams two channels of 24-bit
    baseband data (streamed at 384 ksps). At ~12.25 Mbps of raw throughput (plus overhead),
    this exceeds the reliable capacity of standard 12 Mbps Full-Speed USB.
*   **480 Mbps Capability**: The USB3343 provides a true High-Speed (480 Mbps)
    interface, ensuring that the USB bus is never a bottleneck for
    real-time spectral visualization and audio processing.
*   **Offloading Microcontroller**: Using an external PHY allows the
    STM32H753 to focus its computational resources on DSP and AGC
    tasks rather than managing the low-level physical USB signaling.

# Testing and Twin

The digital twin hardware simulation takes considerable multi-core CPU
horsepower to run. You will want to run this on a pretty high end
modern CPU. I use a 32 core (64 thread) AMD Ryzen Threadripper 2990WX
at 2.2GHz. Fortunately, this level of horsepower is usually only
needed if you don't have hardware. The NexRx app is architected to
require substantially less power (TBD).

To run the "twin" testing against the app (digital twin to stand in
for hardware), you might want to grant realtime capability to your
username. These instructions apply for modern (Ubuntu 26.04 is what
I'm using) Linux.

You can grant non-root users permission to use POSIX realtime
scheduling (like `SCHED_FIFO` or `SCHED_RR`) and memory locking by
configuring PAM (Pluggable Authentication Modules) limits. This is the
standard mechanism to run high-performance audio or SDR DSP code
natively without requiring `sudo`.

1. **Create a Realtime Group:** Terminal.

First, create a dedicated group for users who need realtime execution
privileges.

```bash
sudo groupadd realtime

```

**Verification:** Run `getent group realtime` to confirm the group was
successfully added to the system.


2. **Add Your User to the Group:** Terminal.

Add your current user account to this new group.

```bash
sudo usermod -aG realtime $USER

```

**Verification:** Run `groups $USER` and ensure `realtime` appears in
the output list.


3. **Configure PAM Limits:** File Editing.

Create a new limits configuration file that delegates realtime
priority (`rtprio`) and unlimited memory locking (`memlock`) to
members of the group. The `@` symbol denotes a group rather than a
specific user.

```bash
sudo nano /etc/security/limits.d/99-realtime.conf

```

Paste the following two lines into the file and save it:

```text
@realtime - rtprio 98
@realtime - memlock unlimited

```

**Verification:** Run `cat /etc/security/limits.d/99-realtime.conf` to
ensure the file contains the correct text exactly as written.

4. **Apply and Verify Limits:** Session Restart.

PAM limits are evaluated strictly at login. You must completely log
out of your Ubuntu desktop session and log back in (or reboot) for the
changes to take effect.

**Verification:** Open a new terminal and run `ulimit -r` to confirm
it returns `98`, and `ulimit -l` to confirm it returns `unlimited`.
Your `signalgen` and host application processes can now execute
realtime functions without root access.
