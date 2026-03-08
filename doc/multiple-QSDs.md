# Multiple QSD Architecture

The NexRx architecture employs three independent Quadrature Sampling
Detectors (QSDs) to achieve superior dynamic range, image rejection,
and harmonic suppression. This triple-mixer approach allows the system
to synthesize a "perfect" virtual mixer by combining multiple physical
perspectives.

## The Three QSD Configuration

Each QSD is driven by a local oscillator (LO) derived from a common
master clock but offset to specific frequencies relative to the target
center frequency $f$.

| Mixer | Label | Tuning Frequency | Purpose |
| :--- | :--- | :--- | :--- |
| **QSD 1** | $S_{low}$ | $f - k$ | Lower-side perspective for image rejection and phase reference. |
| **QSD 2** | $S_{high}$ | $f + k$ | Upper-side perspective for image rejection and phase reference. |
| **QSD 3** | $S_{6f}$ | $6f$ | "Sexature" mixer for harmonic sampling and cancellation. |

Where:

* $f$ is the target VFO frequency.

* $k$ is a small frequency offset (typically 12 kHz) used to move the
  baseband signal away from DC 1/f noise and allow for mathematical
  image cancellation.

---

### 1. Fundamental Image Rejection ($f \pm k$)

By tuning two mixers symmetrically around the target frequency, the
system creates two independent baseband representations of the same RF
signal.

* **Lower Mixer ($f-k$):** Sees the target signal at a positive offset
  $+k$.

* **Upper Mixer ($f+k$):** Sees the target signal at a negative offset
  $-k$.

Because these mixers share the same RF signal chain but have different
LO phases, any hardware imbalance (gain or phase mismatch) manifests
differently in each. The host DSP uses **Vector Subtraction** and
**LMS Adaptive Filtering** to correlate these two signals. This allows
the system to mathematically "null" the image response by finding the
complex weight that minimizes the correlation between the signal and
its spectral mirror.

### 2. The Sexature Mixer ($6f$)

The third QSD, designated $S_{6f}$, is tuned to the 6th harmonic of
the fundamental frequency. In a traditional QSD, the switching action
inherently responds to odd harmonics (3rd, 5th, 7th, etc.) because the
switching square wave contains these components.

By sampling at $6f$, the NexRx captures a reference of the harmonic
environment. This "Sexature" data is used to:

1. **Harmonic Cancellation:** Provide a phase and amplitude reference
   to subtract 3rd and 5th harmonic aliases that would otherwise fold
   back into the baseband.

2. **Enhanced Linearity:** By combining the $6f$ perspective with the
   fundamental perspectives using a **1-2-1 weighting matrix**, the
   system can synthesize a response that is mathematically "blind" to
   the 3rd and 5th harmonics.

---

### 3. Signal Combination Matrix

The final I/Q output is synthesized by the host CPU using a weighted
combination of the three physical streams:

    $$S_{out} = w_1 \cdot S_{low} + w_2 \cdot S_{6f} + w_3 \cdot S_{high}$$

In the idealized "1-2-1" mode, the weights are:
* $w_1 = 0.25$
* $w_2 = 0.50$ (Sexature)
* $w_3 = 0.25$

This specific weighting creates a digital "aperture" that
significantly suppresses the response at $3f$ and $5f$, resulting in
an exceptionally clean baseband signal even in the presence of strong
out-of-band interference.

## Digital Twin and Image Rejection Testing

To validate the robustness of the NexRx signal processing, the **Digital Twin** includes a deliberate simulation of hardware imperfections. This allows the host application's compensation algorithms to be tested under controlled, realistic conditions.

### 1. Simulated Hardware Mismatch (SignalGen)

The Digital Twin's `SignalGen` simulates I/Q imbalance for each of the three QSD channels by applying gain and phase errors to the baseband signals before they are digitized and streamed to the host.

*   **Gain Error ($G_e$):** Each channel is initialized with a stable random gain error between $0.95$ and $1.05$ (approximately $\pm 0.4\text{ dB}$).
*   **Phase Error ($\phi_e$):** Each channel includes a stable random phase deviation of up to $\pm 0.05\text{ radians}$ (approximately $\pm 2.8^\circ$).

The resulting baseband signal $B(t)$ for a given channel is generated as:
$$I_{bb}(t) = I_{ideal}(t)$$
$$Q_{bb}(t) = G_e \cdot [Q_{ideal}(t) \cdot \cos(\phi_e) - I_{ideal}(t) \cdot \sin(\phi_e)]$$

This mismatch creates a "ghost" image in the spectrum. For example, a pure USB signal will appear to have a corresponding LSB component at a level determined by the severity of the mismatch (typically $-30$ to $-40\text{ dBc}$ for the simulated errors).

### 2. Adaptive I/Q Correction (DspEngine)

The host application's `DspEngine` employs an independent **Least Mean Squares (LMS)** adaptive filter for each QSD channel to identify and null these imbalances in real-time.

*   **Error Minimization:** The LMS filter attempts to minimize the correlation between the signal and its own complex conjugate (which represents the image frequency).
*   **Correction Logic:** For each channel $S = I + jQ$, the corrected signal $S_{corr}$ is calculated as:
    $$S_{corr} = S - w \cdot S^*$$
    where $w$ is a complex weight tracked by the LMS algorithm.
*   **Update Rule:** The weight $w$ is updated over an integration window (typically $8192$ samples) using a power-normalized rule:
    $$\Delta w = \mu \cdot \frac{\sum (S \cdot S)}{\sum |S|^2}$$
    where $\mu$ is the learning rate.

By applying this correction to $S_{low}$ ($f-k$) and $S_{high}$ ($f+k$) independently *before* they are rotated to DC and combined, the system achieves deep image rejection even when the physical mixers have significant manufacturing tolerances or thermal drift.

---
