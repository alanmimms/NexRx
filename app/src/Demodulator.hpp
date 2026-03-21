/**
 * @file Demodulator.hpp
 * @brief SSB/AM/CW demodulator for I/Q baseband samples
 */

#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <iostream>
#include <atomic>
#include <algorithm>
#include <deque>

class Demodulator {
public:
  enum class Mode { USB = 0, LSB = 1, AM = 2, CW = 3, BYPASS = 4 };

  Demodulator() {
    hilbertHistoryI.resize(hilbertTaps, 0.0f);
    hilbertHistoryQ.resize(hilbertTaps, 0.0f);
    hilbertCoeffs.resize(hilbertTaps, 0.0f);
    computeFilterCoeffs();
    computeHilbertCoeffs();
  }

  void setMode(Mode m) { mode = m; }
  Mode getMode() const { return mode; }

  void setBfoOffset(float hz) { bfoOffset = hz; }
  float getBfoOffset() const { return bfoOffset; }

  void setSampleRate(float rate) {
    sampleRate = rate;
    computeFilterCoeffs();
  }

  void setFilterEnabled(bool enabled) { filterEnabled = enabled; }

  float getSignalLevelRMS() {
    return signalLevelRMS.load(std::memory_order_relaxed);
  }

  // Process a single I/Q sample, returns audio sample
  float process(float i, float q) {
    // Metering
    float magSq = i * i + q * q;
    rmsAccum += magSq;
    sampleAccum++;
    if (sampleAccum >= 960) {
      signalLevelRMS.store(std::sqrt(rmsAccum / sampleAccum), std::memory_order_relaxed);
      rmsAccum = 0;
      sampleAccum = 0;
    }

    // Update Hilbert history for all modes to keep it warm
    hilbertHistoryI[hilbertIndex] = i;
    hilbertHistoryQ[hilbertIndex] = q;

    float audio = 0.0f;

    switch (mode) {
      case Mode::BYPASS: {
        // Just raw I channel for verification
        audio = i;
        break;
      }
      case Mode::AM: {
        float rawEnv = std::sqrt(magSq);
        // Robust AM Demodulator (Envelope + DC Block)
        // Tau ~ 100ms for carrier tracking
        amDcOffset = amDcOffset * 0.9999f + rawEnv * 0.0001f;
        
        // Remove carrier to get audio
        audio = rawEnv - amDcOffset;
        
        // Scale to safe range. 16.0 was too much, but AM envelope 
        // values are smaller than SSB sidebands if carrier is large.
        // Let's use a more moderate gain.
        audio *= 4.0f; 
        break;
      }
      case Mode::CW: {
        float bfoPhaseInc = 2.0f * 3.14159265f * bfoOffset / sampleRate;
        audio = i * std::cos(bfoPhase) + q * std::sin(bfoPhase);
        bfoPhase = std::fmod(bfoPhase + bfoPhaseInc, 2.0f * 3.14159265f);
        audio *= 16.0f;
        break;
      }
      case Mode::USB:
      case Mode::LSB: {
        // Proper phasing SSB using Hilbert transform
        // For USB: Audio = I_delayed - Hilbert(Q)
        // For LSB: Audio = I_delayed + Hilbert(Q)
        
        float hQ = 0.0f;
        int center = (hilbertTaps - 1) / 2;
        for (int n = 0; n < hilbertTaps; n++) {
          int idx = (hilbertIndex + hilbertTaps - n) % hilbertTaps;
          hQ += hilbertHistoryQ[idx] * hilbertCoeffs[n];
        }
        
        int delayedIdx = (hilbertIndex + hilbertTaps - center) % hilbertTaps;
        float iDelayed = hilbertHistoryI[delayedIdx];
        
        audio = (mode == Mode::USB) ? (iDelayed - hQ) : (iDelayed + hQ);
        audio *= 16.0f; // High-fidelity boost for simulated RF chain
        break;
      }
    }

    hilbertIndex = (hilbertIndex + 1) % hilbertTaps;

    if (filterEnabled && mode != Mode::BYPASS) {
      audio = applyBandpassFilter(audio);
    }

    return audio;
  }

private:
  Mode mode = Mode::USB;
  float bfoOffset = 700.0f;
  float sampleRate = 96000.0f;
  float bfoPhase = 0.0f;
  float amDcOffset = 0.0f;
  bool filterEnabled = true;

  std::atomic<float> signalLevelRMS{0.0f};
  double rmsAccum = 0;
  int sampleAccum = 0;

  // Hilbert for phasing
  static constexpr int hilbertTaps = 31;
  std::vector<float> hilbertHistoryI;
  std::vector<float> hilbertHistoryQ;
  std::vector<float> hilbertCoeffs;
  int hilbertIndex = 0;

  void computeHilbertCoeffs() {
    hilbertCoeffs.assign(hilbertTaps, 0.0f);
    int M = (hilbertTaps - 1) / 2;
    const float pi = 3.14159265f;
    for (int n = 0; n < hilbertTaps; n++) {
      if (n == M) {
        hilbertCoeffs[n] = 0;
      } else {
        float x = static_cast<float>(n - M);
        if (static_cast<int>(x) % 2 == 0) {
          hilbertCoeffs[n] = 0;
        } else {
          // h(n) = 2/(pi * (n-M))
          hilbertCoeffs[n] = 2.0f / (pi * x);
          // Apply Hamming window
          float w = 0.54f - 0.46f * std::cos(2.0f * pi * n / (hilbertTaps - 1));
          hilbertCoeffs[n] *= w;
        }
      }
    }
  }

  // Audio bandpass filter
  static constexpr int lpStages = 2;
  static constexpr int hpStages = 1;
  struct BiquadCoeffs { float b0=1, b1=0, b2=0, a1=0, a2=0; };
  struct BiquadState { float z1=0, z2=0; };
  std::array<BiquadCoeffs, hpStages> hpCoeffs;
  std::array<BiquadState, hpStages> hpState;
  std::array<BiquadCoeffs, lpStages> lpCoeffs;
  std::array<BiquadState, lpStages> lpState;

  static float processBiquad(float x, const BiquadCoeffs& c, BiquadState& s) {
    float y = c.b0 * x + s.z1;
    s.z1 = c.b1 * x - c.a1 * y + s.z2;
    s.z2 = c.b2 * x - c.a2 * y;
    return y;
  }

  void computeFilterCoeffs() {
    for(auto& s : hpState) s = {0,0};
    for(auto& s : lpState) s = {0,0};

    const float pi = 3.14159265f;
    float omegaHP = 2.0f * pi * 300.0f / sampleRate;
    float snHP = std::sin(omegaHP), csHP = std::cos(omegaHP);
    float alphaHP = snHP / (2.0f * 0.707f);
    float a0HP = 1.0f + alphaHP;
    // Standard High-pass biquad:
    // b0 = (1 + cos(w))/2, b1 = -(1 + cos(w)), b2 = (1 + cos(w))/2
    // a0 = 1 + alpha, a1 = -2*cos(w), a2 = 1 - alpha
    hpCoeffs[0] = { (1.0f + csHP) / 2.0f / a0HP, 
                    -(1.0f + csHP) / a0HP, 
                    (1.0f + csHP) / 2.0f / a0HP, 
                    -2.0f * csHP / a0HP, 
                    (1.0f - alphaHP) / a0HP };

    float omegaLP = 2.0f * pi * 5000.0f / sampleRate;
    float snLP = std::sin(omegaLP), csLP = std::cos(omegaLP);
    float q[2] = {0.5412f, 1.3065f};
    for (int i=0; i<2; i++) {
      float alpha = snLP / (2.0f * q[i]);
      float a0 = 1.0f + alpha;
      lpCoeffs[i] = {(1.0f-csLP)/2.0f/a0, (1.0f-csLP)/a0, (1.0f-csLP)/2.0f/a0, -2.0f*csLP/a0, (1.0f-alpha)/a0};
    }
  }

  float applyBandpassFilter(float x) {
    float y = x;
    for (int i=0; i<hpStages; i++) y = processBiquad(y, hpCoeffs[i], hpState[i]);
    for (int i=0; i<lpStages; i++) y = processBiquad(y, lpCoeffs[i], lpState[i]);
    return y;
  }
};
