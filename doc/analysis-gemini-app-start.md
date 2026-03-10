# Analysis: gemini-app-start Signal Path

## Twin Signal Generator (signalgen.cpp)
- **Oversampling**: 5x (480 kHz internal rate).
- **Anti-Aliasing**: 6th-order elliptic LPF (fc=40kHz) applied to each of the 6 channels (I/Q for QSD0, QSD1, QSD2) at the 480 kHz rate.
- **Decimation**: Proper 5:1 decimation by taking every 5th sample after the LPF.
- **Quantization**: 24-bit with TPDF (Triangular Probability Density Function) dithering.
- **LO Generation**: Incremental phase rotation (4 muls, 2 adds per sample) for high performance.
- **Phase Continuity**: Resets phases to 1.0/0.0 on frequency changes (discontinuous but stable).
- **Control**: Simple text-based protocol (`SET_LO`).

## App Receive Path DSP (main.cpp:processIQFrame)
- **Frequency Alignment**: 
  - QSD0 (f-k) shifted DOWN by k using incremental rotation phasors.
  - QSD1 (f+k) shifted UP by k using incremental rotation phasors.
- **Image Rejection (LMS)**:
  - Uses QSD2 (sextature reference) as the training target.
  - Convergence: `w0 * QSD0_shifted + w1 * QSD1_shifted ≈ QSD2`.
  - Final Output: `output = w0 * QSD0_shifted + w1 * QSD1_shifted`.
  - Learning Rate (`lmsMu`): 0.001 (very conservative).
- **Demodulation**: Standard SSB/CW demodulation at 96 kHz.
- **Audio Output**: Decimated to 48 kHz by dropping every other sample.
- **Soft Clipping**: Uses `std::tanh` to prevent harsh digital clipping.

## Observations
- The architecture was simple and robust.
- The LMS only matched the fundamental mixers to the reference, and the output was purely the combined fundamental signal.
- Incremental phasors in the app were naturally stable and efficient.
