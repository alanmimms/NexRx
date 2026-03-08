# Triple-QSD Architecture: Theory and Implementation

The NexRx receiver achieves exceptional image rejection and signal integrity by using three independent Quadrature Sampling Detectors (QSDs). This architecture combines multiple physical perspectives of the RF environment to mathematically synthesize a "perfect" virtual mixer.

## 1. Physical Mixer Configuration

Each QSD is tuned to a specific local oscillator (LO) frequency relative to the target VFO frequency $f$:

| Mixer | Label | Tuning Frequency | Purpose |
| :--- | :--- | :--- | :--- |
| **QSD 0** | $S_{low}$ | $f - k$ | Fundamental perspective (shifted UP by $k$ in baseband). |
| **QSD 1** | $S_{high}$ | $f + k$ | Fundamental perspective (shifted DOWN by $k$ in baseband). |
| **QSD 2** | $S_{ref}$ | $f$ | Sextature reference mixer (centered). |

*Typically $k = 12\text{ kHz}$.*

---

## 2. Digital Twin: High-Fidelity Simulation

The Digital Twin (`SignalGen.cpp`) provides a bit-perfect simulation of the hardware signal chain to validate DSP algorithms.

### 5x Oversampling and Anti-Aliasing
To prevent spectral aliasing and ensure realistic behavior, the twin operates at an internal sample rate of **480 kHz** (5x the output rate of 96 kHz). 

1.  **Complex Mixing:** RF signals are mixed to baseband for all three channels at 480 kHz.
2.  **Elliptic Filtering:** A **6th-order elliptic low-pass filter** (fc=40kHz) is applied to each channel. This provides >80dB rejection at the 96kHz folding frequency, ensuring that images and aliases are properly suppressed before decimation.
3.  **TPDF Dithering:** Before 24-bit quantization, Triangular Probability Density Function (TPDF) dithering is applied. This preserves low-level signal linearity and eliminates quantization distortion even at very high attenuation levels.

### Hardware Mismatch Simulation
The twin simulates real-world hardware imperfections by applying gain ($G_e$) and phase ($\phi_e$) offsets to the baseband signals:
$$I_{bb}(t) = I_{ideal}(t)$$
$$Q_{bb}(t) = G_e \cdot [Q_{ideal}(t) \cdot \cos(\phi_e) - I_{ideal}(t) \cdot \sin(\phi_e)]$$

---

## 3. Host DSP: Reference-Guided LMS

The host application (`DspEngine.cpp`) uses the centered **Sextature Mixer (QSD2)** as a reference to calibrate and combine the two fundamental mixers.

### Frequency Alignment
First, the fundamental mixers are shifted back to DC using high-precision digital phasors:
*   $S_0'$ (QSD0) is shifted DOWN by $k$: $S_0 \cdot e^{-j 2 \pi k t}$
*   $S_1'$ (QSD1) is shifted UP by $k$: $S_1 \cdot e^{+j 2 \pi k t}$

### LMS Adaptive Synthesis
Instead of simple averaging, the system trains two complex weights ($w_0, w_1$) to reconstruct the reference signal $S_2$:
$$S_{out} = w_0 S_0' + w_1 S_1'$$

The error vector is defined as the difference between the reconstructed signal and the sextature reference:
$$e = S_2 - S_{out}$$

The weights are updated every sample to minimize the mean square error:
$$\Delta w_n = \mu \cdot e \cdot S_n^*$$

### Why It Works
Because $S_0'$ and $S_1'$ contain the same signal but different image components (due to the opposite frequency shifts), the LMS algorithm naturally finds the weights that align the true signal with $S_2$ while causing the uncorrelated image components to cancel each other out. This "transfers" the inherent image and harmonic rejection of the sextature mixer onto the high-gain fundamental signal path.
