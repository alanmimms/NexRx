/**
 * @file BasebandFilter.cpp
 * @brief Complex FIR bandpass and notch filter implementation
 */

#include "BasebandFilter.hpp"
#include <cmath>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace nexrx {

BasebandFilter::BasebandFilter(float sampleRateIn)
  : sampleRate(sampleRateIn) {
  // Initialize history buffers
  bandpassHistoryI.resize(bandpassTaps, 0.0f);
  bandpassHistoryQ.resize(bandpassTaps, 0.0f);
  notchHistoryI.resize(notchTaps, 0.0f);
  notchHistoryQ.resize(notchTaps, 0.0f);
  bandpassEnabled = true; // Image rejection depends on this
  bandpassCoeffsDirty = true;
}

void BasebandFilter::setBandpassEnabled(bool en) {
  bandpassEnabled = en;
}

void BasebandFilter::setBandpassCenter(float hz) {
  if (bandpassCenter != hz) {
    if (bandpassEnabled && !bandpassCoeffsI.empty()) {
      startBandpassCrossfade();
    }
    bandpassCenter = hz;
    bandpassCoeffsDirty = true;
  }
}

void BasebandFilter::setBandpassWidth(float hz) {
  if (bandpassWidth != hz) {
    if (bandpassEnabled && !bandpassCoeffsI.empty()) {
      startBandpassCrossfade();
    }
    bandpassWidth = std::max(10.0f, hz);  // Minimum 10 Hz
    bandpassCoeffsDirty = true;
  }
}

void BasebandFilter::setBandpassTaps(int taps) {
  // Ensure odd number of taps for symmetric FIR
  taps = taps | 1;
  taps = std::clamp(taps, 15, 1023);

  if (bandpassTaps != taps) {
    if (bandpassEnabled && !bandpassCoeffsI.empty()) {
      startBandpassCrossfade();
    }
    bandpassTaps = taps;
    bandpassHistoryI.assign(taps, 0.0f);
    bandpassHistoryQ.assign(taps, 0.0f);
    bandpassIndex = 0;
    bandpassCoeffsDirty = true;
  }
}

void BasebandFilter::setNotchEnabled(bool en) {
  notchEnabled = en;
}

void BasebandFilter::setNotchCenter(float hz) {
  if (notchCenter != hz) {
    if (notchEnabled && !notchCoeffsI.empty()) {
      startNotchCrossfade();
    }
    notchCenter = hz;
    notchCoeffsDirty = true;
  }
}

void BasebandFilter::setNotchWidth(float hz) {
  if (notchWidth != hz) {
    if (notchEnabled && !notchCoeffsI.empty()) {
      startNotchCrossfade();
    }
    notchWidth = std::max(10.0f, hz);
    notchCoeffsDirty = true;
  }
}

bool BasebandFilter::recompute() {
  bool recomputed = false;

  if (bandpassCoeffsDirty) {
    designBandpass();
    bandpassCoeffsDirty = false;
    recomputed = true;
  }

  if (notchCoeffsDirty) {
    designNotch();
    notchCoeffsDirty = false;
    recomputed = true;
  }

  return recomputed;
}

void BasebandFilter::process(float& i, float& q) {
  // Recompute coefficients if parameters changed
  if (bandpassEnabled && bandpassCoeffsDirty) {
    designBandpass();
    bandpassCoeffsDirty = false;
  }
  if (notchEnabled && notchCoeffsDirty) {
    designNotch();
    notchCoeffsDirty = false;
  }

  if (bandpassEnabled) {
    // Store input in circular history buffer
    bandpassHistoryI[bandpassIndex] = i;
    bandpassHistoryQ[bandpassIndex] = q;

    float outI = 0.0f, outQ = 0.0f;
    size_t idx = bandpassIndex;

    for (int n = 0; n < bandpassTaps; n++) {
      float hi = bandpassHistoryI[idx];
      float hq = bandpassHistoryQ[idx];
      float cI = bandpassCoeffsI[n];
      float cQ = bandpassCoeffsQ[n];

      // Complex multiply: (hi + j*hq) * (cI + j*cQ)
      outI += hi * cI - hq * cQ;
      outQ += hi * cQ + hq * cI;

      if (idx == 0) {
        idx = bandpassTaps - 1;
      } else {
        idx--;
      }
    }

    if (bandpassCrossfading) {
      float oldI, oldQ;
      processOldBandpass(i, q, oldI, oldQ);

      float alpha = static_cast<float>(bandpassCrossfadePos) / kCrossfadeSamples;
      outI = (1.0f - alpha) * oldI + alpha * outI;
      outQ = (1.0f - alpha) * oldQ + alpha * outQ;

      bandpassCrossfadePos++;
      if (bandpassCrossfadePos >= kCrossfadeSamples) {
        bandpassCrossfading = false;
        bandpassOldCoeffsI.clear();
        bandpassOldCoeffsQ.clear();
        bandpassOldHistoryI.clear();
        bandpassOldHistoryQ.clear();
      }
    }

    i = outI;
    q = outQ;
    bandpassIndex = (bandpassIndex + 1) % bandpassTaps;
  }

  if (notchEnabled) {
    notchHistoryI[notchIndex] = i;
    notchHistoryQ[notchIndex] = q;

    float bpI = 0.0f, bpQ = 0.0f;
    size_t idx = notchIndex;

    for (int n = 0; n < notchTaps; n++) {
      float hi = notchHistoryI[idx];
      float hq = notchHistoryQ[idx];
      float cI = notchCoeffsI[n];
      float cQ = notchCoeffsQ[n];

      bpI += hi * cI - hq * cQ;
      bpQ += hi * cQ + hq * cI;

      if (idx == 0) {
        idx = notchTaps - 1;
      } else {
        idx--;
      }
    }

    int delay = notchTaps / 2;
    size_t delayIdx = (notchIndex + notchTaps - delay) % notchTaps;

    float outI = notchHistoryI[delayIdx] - bpI;
    float outQ = notchHistoryQ[delayIdx] - bpQ;

    if (notchCrossfading) {
      float oldI, oldQ;
      processOldNotch(i, q, oldI, oldQ);

      float alpha = static_cast<float>(notchCrossfadePos) / kCrossfadeSamples;
      outI = (1.0f - alpha) * oldI + alpha * outI;
      outQ = (1.0f - alpha) * oldQ + alpha * outQ;

      notchCrossfadePos++;
      if (notchCrossfadePos >= kCrossfadeSamples) {
        notchCrossfading = false;
        notchOldCoeffsI.clear();
        notchOldCoeffsQ.clear();
        notchOldHistoryI.clear();
        notchOldHistoryQ.clear();
      }
    }

    i = outI;
    q = outQ;

    notchIndex = (notchIndex + 1) % notchTaps;
  }
}

void BasebandFilter::designBandpass() {
  bandpassCoeffsI.resize(bandpassTaps);
  bandpassCoeffsQ.resize(bandpassTaps);

  int center = bandpassTaps / 2;
  float cutoff = bandpassWidth / (2.0f * sampleRate); 
  float beta = 6.0f;

  std::vector<float> lpfCoeffs(bandpassTaps);
  float sum = 0;

  for (int n = 0; n < bandpassTaps; n++) {
    int k = n - center;
    float h;
    if (k == 0) {
      h = 2.0f * cutoff;
    } else {
      float x = 2.0f * static_cast<float>(M_PI) * cutoff * static_cast<float>(k);
      h = std::sin(x) / (static_cast<float>(M_PI) * static_cast<float>(k));
    }
    float w = kaiser(n, bandpassTaps, beta);
    lpfCoeffs[n] = h * w;
    sum += lpfCoeffs[n];
  }
  
  // Normalize to unity gain at DC
  if (sum > 0) {
    for (int n = 0; n < bandpassTaps; n++) {
      lpfCoeffs[n] /= sum;
    }
  }

  for (int n = 0; n < bandpassTaps; n++) {
    float phase = 2.0f * static_cast<float>(M_PI) * bandpassCenter * static_cast<float>(n - center) / sampleRate;
    bandpassCoeffsI[n] = lpfCoeffs[n] * std::cos(phase);
    bandpassCoeffsQ[n] = lpfCoeffs[n] * std::sin(phase);
  }
}

void BasebandFilter::designNotch() {
  notchCoeffsI.resize(notchTaps);
  notchCoeffsQ.resize(notchTaps);

  int center = notchTaps / 2;
  float cutoff = notchWidth / (2.0f * sampleRate);
  float beta = 8.0f;

  std::vector<float> lpfCoeffs(notchTaps);
  float sum = 0;

  for (int n = 0; n < notchTaps; n++) {
    int k = n - center;
    float h;
    if (k == 0) {
      h = 2.0f * cutoff;
    } else {
      float x = 2.0f * static_cast<float>(M_PI) * cutoff * static_cast<float>(k);
      h = std::sin(x) / (static_cast<float>(M_PI) * static_cast<float>(k));
    }
    float w = kaiser(n, notchTaps, beta);
    lpfCoeffs[n] = h * w;
    sum += lpfCoeffs[n];
  }
  
  // Normalize
  if (sum > 0) {
    for (int n = 0; n < notchTaps; n++) {
      lpfCoeffs[n] /= sum;
    }
  }

  for (int n = 0; n < notchTaps; n++) {
    float phase = 2.0f * static_cast<float>(M_PI) * notchCenter * static_cast<float>(n - center) / sampleRate;
    notchCoeffsI[n] = lpfCoeffs[n] * std::cos(phase);
    notchCoeffsQ[n] = lpfCoeffs[n] * std::sin(phase);
  }
}

float BasebandFilter::kaiser(int n, int N, float beta) {
  float M = (static_cast<float>(N) - 1.0f) / 2.0f;
  float ratio = (static_cast<float>(n) - M) / M;
  float arg = beta * std::sqrt(std::max(0.0f, 1.0f - ratio * ratio));
  return besselI0(arg) / besselI0(beta);
}

float BasebandFilter::besselI0(float x) {
  float sum = 1.0f;
  float term = 1.0f;
  float x2 = x * x / 4.0f;

  for (int k = 1; k <= 25; k++) {
    term *= x2 / (static_cast<float>(k) * static_cast<float>(k));
    sum += term;
    if (term < 1e-10f * sum) {
      break;
    }
  }

  return sum;
}

void BasebandFilter::startBandpassCrossfade() {
  bandpassOldCoeffsI = bandpassCoeffsI;
  bandpassOldCoeffsQ = bandpassCoeffsQ;
  bandpassOldHistoryI = bandpassHistoryI;
  bandpassOldHistoryQ = bandpassHistoryQ;
  bandpassOldIndex = bandpassIndex;
  bandpassOldTaps = bandpassTaps;
  bandpassCrossfadePos = 0;
  bandpassCrossfading = true;
}

void BasebandFilter::startNotchCrossfade() {
  notchOldCoeffsI = notchCoeffsI;
  notchOldCoeffsQ = notchCoeffsQ;
  notchOldHistoryI = notchHistoryI;
  notchOldHistoryQ = notchHistoryQ;
  notchOldIndex = notchIndex;
  notchCrossfadePos = 0;
  notchCrossfading = true;
}

void BasebandFilter::processOldBandpass(float i, float q, float& outI, float& outQ) {
  if (bandpassOldCoeffsI.empty() || bandpassOldTaps == 0) {
    outI = i;
    outQ = q;
    return;
  }

  bandpassOldHistoryI[bandpassOldIndex] = i;
  bandpassOldHistoryQ[bandpassOldIndex] = q;

  outI = 0.0f;
  outQ = 0.0f;
  size_t idx = bandpassOldIndex;

  for (int n = 0; n < bandpassOldTaps; n++) {
    float hi = bandpassOldHistoryI[idx];
    float hq = bandpassOldHistoryQ[idx];
    float cI = bandpassOldCoeffsI[n];
    float cQ = bandpassOldCoeffsQ[n];

    outI += hi * cI - hq * cQ;
    outQ += hi * cQ + hq * cI;

    if (idx == 0) {
      idx = bandpassOldTaps - 1;
    } else {
      idx--;
    }
  }

  bandpassOldIndex = (bandpassOldIndex + 1) % bandpassOldTaps;
}

void BasebandFilter::processOldNotch(float i, float q, float& outI, float& outQ) {
  if (notchOldCoeffsI.empty()) {
    outI = i;
    outQ = q;
    return;
  }

  int oldTaps = static_cast<int>(notchOldCoeffsI.size());
  notchOldHistoryI[notchOldIndex] = i;
  notchOldHistoryQ[notchOldIndex] = q;

  float bpI = 0.0f, bpQ = 0.0f;
  size_t idx = notchOldIndex;

  for (int n = 0; n < oldTaps; n++) {
    float hi = notchOldHistoryI[idx];
    float hq = notchOldHistoryQ[idx];
    float cI = notchOldCoeffsI[n];
    float cQ = notchOldCoeffsQ[n];

    bpI += hi * cI - hq * cQ;
    bpQ += hi * cQ + hq * cI;

    if (idx == 0) {
      idx = oldTaps - 1;
    } else {
      idx--;
    }
  }

  int delay = oldTaps / 2;
  size_t delayIdx = (notchOldIndex + oldTaps - delay) % oldTaps;

  outI = notchOldHistoryI[delayIdx] - bpI;
  outQ = notchOldHistoryQ[delayIdx] - bpQ;

  notchOldIndex = (notchOldIndex + 1) % oldTaps;
}

} // namespace nexrx
