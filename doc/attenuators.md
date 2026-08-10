# Attenuator T-Pad Values

In a T-pad topology, you use two identical **Series** resistors (R1,
R2) and one **Shunt** resistor (R3) to ground.

| Target Attenuation | Ideal Series | Ideal Shunt | Recommended E96 Series | Recommended E96 Shunt |
| --- | --- | --- | --- | --- |
| **3 dB** | $8.55\ \Omega$ | $141.93\ \Omega$ | **$8.66\ \Omega$** | **$143\ \Omega$** |
| **6 dB** | $16.61\ \Omega$ | $66.93\ \Omega$ | **$16.5\ \Omega$** | **$66.5\ \Omega$** |
| **12 dB** | $29.92\ \Omega$ | $26.81\ \Omega$ | **$30.1\ \Omega$** | **$26.7\ \Omega$** |
| **24 dB** | $44.06\ \Omega$ | $6.34\ \Omega$ | **$44.2\ \Omega$** | **$6.34\ \Omega$** |

[Explanation of Method] These specific E96 standard values keep the
real-world VSWR basically flawless (e.g., using $16.5\ \Omega$ and
$66.5\ \Omega$ for a 6 dB pad results in an actual characteristic
impedance of $49.8\ \Omega$ and an actual attenuation of $6.02\text{
dB}$). Standard trace tolerance in the PCB will introduce more
variance than this rounding!
