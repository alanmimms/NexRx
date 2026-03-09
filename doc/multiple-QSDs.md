# Triple-QSD Architecture: Theory and Implementation

The NexRx receiver achieves exceptional image rejection and signal
integrity by using three independent Quadrature Sampling Detectors
(QSDs). This architecture combines multiple physical perspectives of the
RF environment to mathematically synthesize a "perfect" virtual mixer.

## 1. Physical Mixer Configuration

Each QSD is tuned to a specific local oscillator (LO) frequency relative
to the target VFO frequency $f$:

| Mixer | Label | Tuning Frequency | Purpose |
| :--- | :--- | :--- | :--- |
| **QSD 0** | $S_{low}$ | $f - k$ | Fundamental perspective (shifted UP by $k$ in baseband). |
| **QSD 1** | $S_{high}$ | $f + k$ | Fundamental perspective (shifted DOWN by $k$ in baseband). |
| **QSD 2** | $S_{ref}$ | $f$ | Sextature reference mixer (centered). |

*Typically $k = 12\text{ kHz}$.*

---

## 2. Digital Twin: High-Fidelity Simulation

The Digital Twin (`SignalGen.cpp`) provides a bit-perfect simulation of
the hardware signal chain to validate DSP algorithms.

### 5x Oversampling and Anti-Aliasing
To prevent spectral aliasing and ensure realistic behavior, the twin
operates at an internal sample rate of **480 kHz** (5x the output rate
of 96 kHz).

1.  **Complex Mixing:** RF signals are mixed to baseband for all three
    channels at 480 kHz.
2.  **Elliptic Filtering:** A **6th-order elliptic low-pass filter**
    (fc=40kHz) is applied to each channel. This provides >80dB
    rejection at the 96kHz folding frequency, ensuring that images and
    aliases are properly suppressed before decimation.
3.  **TPDF Dithering:** Before 24-bit quantization, Triangular
    Probability Density Function (TPDF) dithering is applied. This
    preserves low-level signal linearity and eliminates quantization
    distortion even at very high attenuation levels.

### Hardware Mismatch Simulation
The twin simulates real-world hardware imperfections by applying gain
($G_e$) and phase ($\phi_e$) offsets to the baseband signals:

	$$I_{bb}(t) = I_{ideal}(t)$$
	$$Q_{bb}(t) = G_e \cdot [Q_{ideal}(t) \cdot \cos(\phi_e) - I_{ideal}(t) \cdot \sin(\phi_e)]$$

---

## 3. Host DSP: Reference-Guided LMS

The host application (`DspEngine.cpp`) uses the centered **Sextature
Mixer (QSD2)** as a reference to calibrate and combine the two
fundamental mixers.

### Frequency Alignment
First, the fundamental mixers are shifted back to DC using
high-precision digital phasors:

*   $S_0'$ (QSD0) is shifted DOWN by $k$: $S_0 \cdot e^{-j 2 \pi k t}$
*   $S_1'$ (QSD1) is shifted UP by $k$: $S_1 \cdot e^{+j 2 \pi k t}$

### One-Time High-Precision Calibration

While hardware components (transformers, switches, capacitors) are relatively stable once at operating temperature, their initial manufacturing tolerances and the group delay of the anti-alias filters require precise calibration to achieve maximum image rejection.

#### 1. Why Calibration is Required
Each QSD is a separate physical device. To mathematically synthesize a "perfect" virtual mixer, the system must compensate for:
*   **Independent I/Q Mismatches:** Unique gain and phase errors internal to each of the three physical mixers.
*   **Filter Group Delay:** The 6th-order elliptic filters introduce significant phase shifts (typically ~33°) that must be perfectly aligned to allow the $S_0/S_1$ pair to reconstruct the $S_2$ reference.

#### 2. The Calibration Workflow (The "CALIBRATE" Button)
Calibration is performed on-demand rather than continuously to ensure maximum signal integrity and stability.
1.  **Trigger:** Clicking the **CALIBRATE** button in the GUI sends a `CAL!` command to the twin (or eventually the hardware).
2.  **Clean Stimulus:** The twin immediately pauses all regular RF stimulus (voice, noise, etc.) and generates a pure 10mV calibration tone at $VFO + 1\text{kHz}$ for a fixed 2-second window.
3.  **Least Squares Solver:** The host DSP switches from normal processing to a high-precision vector accumulation mode. Over a window of 32,744 samples, it solves for the exact complex weights ($w_n$) that minimize the error $e = S_2 - (w_0 S_0' + w_1 S_1')$.
4.  **Verification:** The derived hardware errors (Gain/Phase) and alignment phases are reported to the **application console log**.

#### 3. Persistence and Application
Once the calibration window closes, the results are formatted as a Lua table and can be appended to `config/calibration.lua`.
*   **Default:** If no calibration file exists, the app defaults to zero compensation.
*   **Startup:** At launch, `Calibration.lua` loads the stored offsets and applies them to the `DspEngine` as static compensation parameters.
*   **Performance:** By using an exact mathematical solver rather than iterative descent, the system achieves an extremely deep, stable null that is preserved across sessions.

---
