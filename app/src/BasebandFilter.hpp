/**
 * @file BasebandFilter.hpp
 * @brief Complex FIR bandpass and notch filters for baseband I/Q processing
 *
 * Provides dynamically adjustable filters with:
 * - Linear phase (FIR)
 * - Arbitrary center frequency and bandwidth
 * - Adjustable steepness via tap count
 */

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace nexrx {

class BasebandFilter {
public:
  explicit BasebandFilter(float sampleRate = 96000.0f);

  // Process complex I/Q sample (in-place)
  void process(float& i, float& q);

  // Bandpass control
  void setBandpassEnabled(bool en);
  void setBandpassCenter(float hz);   // Offset from DC (-fs/2 to +fs/2)
  void setBandpassWidth(float hz);    // 3dB bandwidth
  void setBandpassTaps(int taps);     // Filter length (must be odd)

  // Notch control
  void setNotchEnabled(bool en);
  void setNotchCenter(float hz);      // Offset from DC
  void setNotchWidth(float hz);       // Notch bandwidth

  /// Force coefficient recomputation if parameters have changed.
  /// @return true if any coefficients were recomputed
  bool recompute();

  // Getters
  [[nodiscard]] bool isBandpassEnabled() const { return bandpassEnabled; }
  [[nodiscard]] float getBandpassCenter() const { return bandpassCenter; }
  [[nodiscard]] float getBandpassWidth() const { return bandpassWidth; }
  [[nodiscard]] int getBandpassTaps() const { return bandpassTaps; }

  [[nodiscard]] bool isNotchEnabled() const { return notchEnabled; }
  [[nodiscard]] float getNotchCenter() const { return notchCenter; }
  [[nodiscard]] float getNotchWidth() const { return notchWidth; }

private:
  float sampleRate;

  // Bandpass FIR state
  bool bandpassEnabled = false;
  float bandpassCenter = 700.0f;
  float bandpassWidth = 500.0f;
  int bandpassTaps = 127;
  std::vector<float> bandpassCoeffsI;      // Complex FIR coefficients (I component)
  std::vector<float> bandpassCoeffsQ;      // Complex FIR coefficients (Q component)
  std::vector<float> bandpassHistoryI;     // Circular delay line for I
  std::vector<float> bandpassHistoryQ;     // Circular delay line for Q
  size_t bandpassIndex = 0;
  bool bandpassCoeffsDirty = true;

  // Notch FIR state
  bool notchEnabled = false;
  float notchCenter = 0.0f;
  float notchWidth = 100.0f;
  int notchTaps = 63;
  std::vector<float> notchCoeffsI;
  std::vector<float> notchCoeffsQ;
  std::vector<float> notchHistoryI;
  std::vector<float> notchHistoryQ;
  size_t notchIndex = 0;
  bool notchCoeffsDirty = true;

  // Crossfade state for smooth transitions
  static constexpr int kCrossfadeSamples = 960;  // 10ms at 96kHz
  bool bandpassCrossfading = false;
  int bandpassCrossfadePos = 0;
  std::vector<float> bandpassOldCoeffsI;
  std::vector<float> bandpassOldCoeffsQ;
  std::vector<float> bandpassOldHistoryI;
  std::vector<float> bandpassOldHistoryQ;
  size_t bandpassOldIndex = 0;
  int bandpassOldTaps = 0;

  bool notchCrossfading = false;
  int notchCrossfadePos = 0;
  std::vector<float> notchOldCoeffsI;
  std::vector<float> notchOldCoeffsQ;
  std::vector<float> notchOldHistoryI;
  std::vector<float> notchOldHistoryQ;
  size_t notchOldIndex = 0;

  // Filter design functions
  void designBandpass();
  void designNotch();

  // Start crossfade from current to new coefficients
  void startBandpassCrossfade();
  void startNotchCrossfade();

  // Process with old coefficients (during crossfade)
  void processOldBandpass(float i, float q, float& outI, float& outQ);
  void processOldNotch(float i, float q, float& outI, float& outQ);

  // Kaiser window function
  static float kaiser(int n, int N, float beta);

  // Modified Bessel function I0 (for Kaiser window)
  static float besselI0(float x);
};

} // namespace nexrx
