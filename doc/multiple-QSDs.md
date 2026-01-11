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
