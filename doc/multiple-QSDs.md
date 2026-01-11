To truly eliminate the image, the host DSP doesn't just average them; it uses the two perspectives to perform **Image Rejection via Vector Subtraction**.

---

### 1. The Geometry of the Image Signal

In a complex I/Q system, the "real" signal () and the "image" signal () are related by their rotation. If you tune to a frequency , the signal at  is your target, but due to hardware imperfections (gain/phase imbalance), a signal at  will "leak" into your passband as an image.

Because you have two mixers ( and ) offset by , the "leakage" happens at different relative offsets in each:

* **Mixer A ():** The image of a signal at  appears at a specific phase relative to the .
* **Mixer B ():** The same  signal appears at a **different phase** because the  is on the other side of the carrier.

### 2. The Vector Math: LMS Adaptive Correction

Instead of a simple average, the host app uses an **LMS (Least Mean Squares) Adaptive Filter**. This is the standard "secret sauce" for high-end SDRs.

1. **Correlation Check:** The DSP looks for components in the I/Q stream that are correlated between the two offset mixers.
2. **The Error Vector:** If a signal is "real," it will align perfectly once the digital rotation () is applied. If it is an "image," the rotation will cause the image vectors from Mixer A and Mixer B to point in different directions.
3. **The Nulling Equation:** The host calculates a complex coefficient () to "weight" Mixer B before subtracting it from Mixer A:



The DSP engine continuously adjusts  to minimize the energy of the image.

### 3. Why Six Channels (Triple-QSD) is Better

By having the  "Sexature" channel (Mixer C), you add a third point of reference.

* **Mixers A & B:** Solve for the Fundamental Image (the  balance).
* **Mixer C ():** Acts as the "Harmonic Probe." It specifically captures the phase of the 3rd and 5th harmonics.

The math then becomes a **Matrix Operation** rather than a simple subtraction:



Where  is a correction matrix computed by your host CPU's vector unit. This matrix doesn't just average; it **re-projects** the six-dimensional data onto a single clean I/Q plane where the harmonics and images have been mathematically rotated into "zero-length" vectors.


# Tuning
To implement the Least Mean Squares (LMS) adaptive correction for
image rejection in the host DSP, we treat the system as a real-time
optimization problem. The goal is to continuously calculate a complex
coefficient that perfectly cancels the "leakage" from the opposite
side of the spectrum.

### 1. The Adaptive Correction Equation

In your host app, you have the centered signals (from Mixer A) and
(from Mixer B). Because of hardware imbalances, Mixer A contains the
true signal plus an unwanted image . Mixer B contains the same
components but with a different phase relationship.

The host creates a "clean" signal by applying a complex weight to
Mixer B and subtracting it from Mixer A:


### 2. The LMS Update Logic

The "Learning" part happens by calculating an **Error Vector**. The
host assumes that in a perfectly balanced system, there should be zero
correlation between the signal and its image.

The weight is updated for every new sample (or block of samples) using
the following logic:

1. **Calculate the Error:** In this case, the output itself acts as
   the error signal for the image rejection loop.

2. **Update the Weight:**


*  (Mu): The **step size** or learning rate. A small makes the radio
   stable but slow to adapt to thermal drift; a large reacts instantly
   but can introduce "hunting" noise.

* : The **complex conjugate** of the Mixer B signal.



---

### 3. Vectorized Implementation (AVX/SIMD)

Since you are targeting a modern PC with vector processing, you don't
calculate this sample-by-sample in a slow loop. Instead, you use
**Block-LMS**.

| Step | Operation | Vector Instruction |
| --- | --- | --- |
| **Dot Product** | Multiply a block of  samples by the current weight . | `VFMADD` (Fused Multiply-Add) |
| **Subtraction** | Subtract the weighted  from  to get a block of . | `VSUB` (Vector Subtract) |
| **Accumulation** | Correlate the error  with  to find the average gradient. | `VDPPS` (Dot Product) |
| **Weight Update** | Update the single complex value of  for the next block. | Scalar math on the result |

### 4. Why this handles Hardware Drift

Hardware components like the **TS3A4751** switches or the
**BN-43-202** core in the Pentafilar transformer will change their
electrical characteristics as the NexRig heats up during a long 100W
transmission.

* **Thermal Drift:** As the transformer permeability shifts, the phase
  balance between Mixers A and B moves.

* **LMS Tracking:** Because the host app runs the update equation
  thousands of times per second, it "tracks" this drift in real-time.

* **Result:** The image rejection stays "pinned" at its maximum depth
  (often >70dB) regardless of whether the radio is cold or at its
  thermal limit.

---

### 5. Final Harmonic Nulling (Matrix Mode)

While the LMS handles the image, the **1-2-1 weighting** is applied as
a static (or semi-static) matrix multiplication across the three
mixers (Sa, Sb, S6f).

$$\begin{bmatrix} I \\ Q \end{bmatrix}_{out} = \mathbf{w}_{LMS} \cdot \left( 0.25 S_A + 0.5 S_{6f} + 0.25 S_B \right)$$

By nesting the LMS correction inside the 1-2-1 combiner, you create a
digital signal path that is mathematically "blind" to the 3rd and 5th
harmonics while being perfectly balanced for the fundamental.

Would you like me to draft the **C++ data structures** for the 6-channel input buffer to ensure your vector units can load the data with zero memory alignment penalties?
