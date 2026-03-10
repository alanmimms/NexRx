# Analysis: Current Workspace Signal Path

## Twin Signal Generator (SignalGen.cpp)
- **Oversampling**: 5x (480 kHz rate).
- **Anti-Aliasing**: 6th-order elliptic filter (fc=40kHz) at 480 kHz.
- **Decimation**: Proper 5:1 decimation (no boxcar averaging found in the latest code).
- **Quantization**: 24-bit with TPDF dithering.
- **LO Generation**: Incremental phase rotation with **normalization every sample** (`std::sqrt` and `1.0/mag`).
- **Phase Continuity**: Maintained during frequency updates (no reset).
- **Simulated Hardware Errors**: Applied at the 480 kHz rate.
- **Control**: CBOR-based protocol (`SVFO`, `SPRL`, etc.).

## App Control Path (GuiEngine.cpp & AppController.lua)
- **VFO Tuning**: `rx.setVFO` is called via the Model's reactive system.
- **VFO Offset**: Correctly sends 12000.0 Hz (12kHz * 1000) for the default offset.
- **Issue**: There is a potential race or miscommunication where `SVFO` is not seen in logs, while `SPRL/SPRC` are. This might be due to the `rx` global not being correctly accessed or the `postCommand` mechanism in `GuiEngine.cpp`.

## App Receive Path DSP (DspEngine.cpp)
- **Frequency Alignment**: Uses `std::fmod`, `std::cos`, and `std::sin` every sample based on `totalSamplesProcessed`. This is expensive and susceptible to phase jumps if packets are dropped.
- **Image Rejection (LMS)**:
  - Uses a hard-coded "Triple-QSD Matrix" (1-2-1): `output = 0.5 * (w0*S0' + w1*S1') + 0.5 * S2`.
  - Learning Rate (`mu`): 0.05 (50x larger than gemini-app-start). This high rate likely causes the LMS to correlate on noise and "chatter," creating the reported noise floor artifacts.
  - Leaky LMS: Adds a `1.0 - leak` term which can also add noise if not tuned carefully.
- **Matrix Bypass**: Allows raw S2 output.

## Root Cause Comparison
1. **Noise Floor**: Highly likely caused by the high LMS learning rate (0.05) and the 1-2-1 matrix combination which spreads LMS chatter across the spectrum.
2. **Tuning Glitches**: Not present in Twin (incremental rotation is smooth), but potential timing issues in App due to `totalSamplesProcessed` reference.
3. **VFO Command Failure**: Likely a Lua-to-C++ binding or Model update synchronization issue.
