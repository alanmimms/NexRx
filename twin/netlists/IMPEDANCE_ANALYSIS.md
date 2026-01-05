# NexRx QSD Impedance Analysis

## Summary

The digital twin simulations reveal a significant impedance mismatch between the hexafilar transformer secondary and the QSD input. This document summarizes the findings and implications.

## Key Measurements

### Source Impedance Study (33mV RF @ 14.01MHz, LO @ 14MHz)

| Source Impedance | I/Q Baseband pk-pk | Relative to Ideal |
|------------------|-------------------|-------------------|
| 0.5Ω (ideal)     | 27.0 mV           | 100%              |
| 2Ω (buffered)    | 13.3 mV           | 49%               |
| 22Ω (hexafilar)  | 1.7 mV            | 6.3%              |

### Hexafilar Transformer Loading

- Expected voltage ratio: 3:1 (200Ω : 22Ω)
- Measured voltage ratio: **16:1** (loaded by QSD)
- Conversion gain: -8.8 dB (baseband relative to RF at secondary)

## Root Cause

The QSD operates as a switched-capacitor circuit. During each LO phase (~18ns at 14MHz), one 33nF sampling capacitor connects to the RF input. The RC time constant determines charge transfer:

```
τ = R_source × C_samp = 22Ω × 33nF = 726ns
T_phase = T_LO/4 = 17.9ns (at 14MHz)
Charge per phase: ~2.5% of full charge (exp(-18/726) ≈ 0.975)
```

The capacitor cannot fully charge within each sampling window, resulting in:
1. Heavy loading of the transformer secondary
2. Reduced effective voltage transfer
3. Lower conversion gain

## Implications

The hexafilar's 22Ω secondary impedance is approximately **40x higher** than optimal for voltage-mode QSD operation at 14MHz with 33nF caps.

For proper voltage tracking, the source impedance should satisfy:
```
R_source × C_samp << T_LO/4
R_source << 17.9ns / 33nF = 0.54Ω
```

## Possible Solutions

### 1. Lower Source Impedance
Add RF buffer amplifiers between transformer and QSD with <1Ω output impedance.
- Pros: Maintains design intent
- Cons: Adds components, noise, power

### 2. Smaller Sampling Capacitors
Reduce from 33nF to ~330pF allows 22Ω source.
- Pros: No additional components
- Cons: Baseband BW increases to ~22MHz (too wide), higher thermal noise

### 3. Different Transformer Ratio
Design for lower secondary impedance.
- With 9:1 impedance ratio instead of current design
- Secondary Z = 200/9 ≈ 22Ω matches current design
- May require lower primary impedance

### 4. Accept Current-Mode Operation
The QSD works but with reduced conversion gain. Compensate in downstream amplification/DSP.
- Measured: -8.8 dB conversion gain
- Pros: No hardware changes
- Cons: Reduced dynamic range, higher noise figure

## Recommendations

For the digital twin:
1. The current models accurately represent the real-world impedance mismatch
2. Continue with current model for accurate simulation
3. Add downstream gain stages to model complete signal chain
4. Consider modeling the MAX9939 differential amplifier stage

For hardware design review:
1. Verify actual QSD conversion gain matches simulation
2. Consider adding buffer amplifiers if sensitivity is inadequate
3. Evaluate noise figure impact of the impedance mismatch

## Test Files

- `qsd_impedance_study.cir` - Compares 0.5Ω, 2Ω, 22Ω sources
- `hexafilar_qsd_test.cir` - Full hexafilar + QSD verification
- `qsd_baseband_test2.cir` - QSD with low-impedance source (working)

---
*Analysis performed: January 2026*
*Digital Twin version: Pre-release*
