// NexRx Digital Twin - RF Capture Player
//
// Plays back recorded I/Q data as RF stimulus.
// Supports raw binary and common SDR file formats.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "AntennaStimulus.hpp"

#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace nexrx {

//======================================================================
// I/Q Sample Format
//======================================================================
enum class IQFormat {
  FLOAT32_IQ,     // Interleaved float32 I, Q (SDR# format)
  FLOAT64_IQ,     // Interleaved float64 I, Q
  INT16_IQ,       // Interleaved int16 I, Q (RTL-SDR format)
  INT8_IQ,        // Interleaved int8 I, Q (RTL-SDR raw)
  UINT8_IQ,       // Interleaved uint8 I, Q (offset binary)
  COMPLEX_FLOAT32 // std::complex<float> (GNU Radio)
};

//======================================================================
// RF Capture Player
//
// Plays back recorded I/Q data, upconverting to RF.
// The I/Q data represents baseband; this class generates:
//   V(t) = I(t)*cos(2*pi*f_c*t) - Q(t)*sin(2*pi*f_c*t)
//
// For direct RF recordings (no upconversion needed), set
// centerFrequency to 0.
//======================================================================
class RFCapturePlayer : public AntennaStimulus {
public:
  RFCapturePlayer() = default;

  // Load I/Q data from WAV file
  bool loadWav(const std::string& pathIn) {
    std::string tryPaths[] = { pathIn, "twin/" + pathIn, "../" + pathIn, "../../" + pathIn };
    std::ifstream file;
    std::string p;

    for (const auto& tp : tryPaths) {
      file.open(tp, std::ios::binary);
      if (file.is_open()) {
        p = tp;
        break;
      }
    }

    if (!file.is_open()) {
      return false;
    }

    char header[44];
    file.read(header, 44);
    if (file.gcount() < 44) return false;

    if (std::strncmp(header, "RIFF", 4) != 0 || std::strncmp(header + 8, "WAVE", 4) != 0) {
      return false;
    }

    // Basic WAV parsing
    uint16_t audioFormat = *reinterpret_cast<uint16_t*>(header + 20);
    uint16_t numChannels = *reinterpret_cast<uint16_t*>(header + 22);
    uint32_t sampleRate = *reinterpret_cast<uint32_t*>(header + 24);
    uint16_t bitsPerSample = *reinterpret_cast<uint16_t*>(header + 34);

    sampleRateHz = static_cast<double>(sampleRate);
    samplePeriodS = 1.0 / sampleRateHz;

    // Skip to data chunk
    file.seekg(12);
    bool foundData = false;
    while (file.good()) {
      char chunkId[4];
      uint32_t chunkSize;
      file.read(chunkId, 4);
      file.read(reinterpret_cast<char*>(&chunkSize), 4);
      if (std::strncmp(chunkId, "data", 4) == 0) {
        size_t bytesPerSample = bitsPerSample / 8;
        size_t numSamples = chunkSize / (bytesPerSample * numChannels);
        samplesI.resize(numSamples);
        samplesQ.resize(numSamples);

        for (int i = 0; i < (int)numSamples; ++i) {
          double valI = 0, valQ = 0;
          
          auto readSample = [&](double& out) {
            if (audioFormat == 3 && bitsPerSample == 32) {
              // IEEE Float
              float s; file.read(reinterpret_cast<char*>(&s), 4);
              out = s;
            } else if (audioFormat == 1) {
              // PCM Integer
              if (bitsPerSample == 16) {
                int16_t s; file.read(reinterpret_cast<char*>(&s), 2);
                out = s / 32768.0;
              } else if (bitsPerSample == 8) {
                uint8_t s; file.read(reinterpret_cast<char*>(&s), 1);
                out = (s - 128) / 128.0;
              } else if (bitsPerSample == 24) {
                uint8_t b[3]; file.read(reinterpret_cast<char*>(b), 3);
                int32_t s = (b[2] << 24) | (b[1] << 16) | (b[0] << 8);
                out = s / 2147483648.0;
              } else if (bitsPerSample == 32) {
                int32_t s; file.read(reinterpret_cast<char*>(&s), 4);
                out = s / 2147483648.0;
              }
            }
          };

          readSample(valI);
          if (numChannels > 1) {
            readSample(valQ);
            // Skip remaining channels if any
            if (numChannels > 2) {
              file.seekg(bytesPerSample * (numChannels - 2), std::ios::cur);
            }
          }
          
          samplesI[i] = valI;
          samplesQ[i] = valQ;
        }
        foundData = true;
        break;
      }
      file.seekg(chunkSize, std::ios::cur);
    }

    path = p;
    return foundData && !samplesI.empty();
  }

  // Load I/Q data from file
  bool loadFile(const std::string& p, IQFormat format, double sampleRateHzIn) {
    sampleRateHz = sampleRateHzIn;
    samplePeriodS = 1.0 / sampleRateHz;

    std::ifstream file(p, std::ios::binary);
    if (!file) {
      return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    samplesI.clear();
    samplesQ.clear();

    switch (format) {
      case IQFormat::FLOAT32_IQ:
      case IQFormat::COMPLEX_FLOAT32:
        loadFloat32IQ(file, fileSize);
        break;
      case IQFormat::FLOAT64_IQ:
        loadFloat64IQ(file, fileSize);
        break;
      case IQFormat::INT16_IQ:
        loadInt16IQ(file, fileSize);
        break;
      case IQFormat::INT8_IQ:
        loadInt8IQ(file, fileSize);
        break;
      case IQFormat::UINT8_IQ:
        loadUint8IQ(file, fileSize);
        break;
    }

    path = p;
    return !samplesI.empty();
  }

  // Load from raw float vectors (for programmatic use)
  void loadData(const std::vector<float>& iSamplesIn,
                const std::vector<float>& qSamplesIn,
                double sampleRateHzIn) {
    samplesI.assign(iSamplesIn.begin(), iSamplesIn.end());
    samplesQ.assign(qSamplesIn.begin(), qSamplesIn.end());
    sampleRateHz = sampleRateHzIn;
    samplePeriodS = 1.0 / sampleRateHz;
    path = "(memory)";
  }

  // Set center frequency for upconversion (0 = baseband playback)
  void setCenterFrequency(double freqHz) {
    centerFreqHz = freqHz;
  }

  // Set amplitude scaling
  void setAmplitude(double ampV) {
    amplitudeV = ampV;
  }

  // Enable/disable looping
  void setLooping(bool l) {
    loop = l;
  }

  // Swap I and Q channels
  void setSwapIQ(bool s) {
    swapIQ = s;
  }

  [[nodiscard]] double getSample(double timeS) const override {
    if (samplesI.empty()) {
      return 0.0;
    }

    // Calculate sample index
    double sampleIdxF = timeS * sampleRateHz;
    size_t totalSamples = samplesI.size();

    if (loop) {
      sampleIdxF = std::fmod(sampleIdxF, static_cast<double>(totalSamples));
      if (sampleIdxF < 0) {
        sampleIdxF += totalSamples;
      }
    } else if (sampleIdxF >= totalSamples || sampleIdxF < 0) {
      return 0.0;
    }

    // Linear interpolation between samples
    size_t idx0 = static_cast<size_t>(sampleIdxF);
    size_t idx1 = (idx0 + 1) % totalSamples;
    double frac = sampleIdxF - idx0;

    double iVal = samplesI[idx0] * (1.0 - frac) + samplesI[idx1] * frac;
    double qVal = samplesQ[idx0] * (1.0 - frac) + samplesQ[idx1] * frac;

    if (swapIQ) {
      std::swap(iVal, qVal);
    }

    // Upconvert to RF if center frequency is set
    double output;
    if (centerFreqHz > 0) {
      double phase = 2.0 * M_PI * centerFreqHz * timeS;
      output = iVal * std::cos(phase) - qVal * std::sin(phase);
    } else {
      // Baseband: just return I component (or could return magnitude)
      output = iVal;
    }

    return output * amplitudeV;
  }

  [[nodiscard]] std::string description() const override {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "RFCapture(" << path << ", "
        << samplesI.size() << " samples, "
        << sampleRateHz / 1e6 << " MHz rate";
    if (centerFreqHz > 0) {
      oss << ", fc=" << centerFreqHz / 1e6 << " MHz";
    }
    oss << ")";
    return oss.str();
  }

  void reset() override {
    // Nothing to reset for stateless playback
  }

  [[nodiscard]] bool hasMore(double timeS) const override {
    if (loop) {
      return true;
    }
    double sampleIdx = timeS * sampleRateHz;
    return sampleIdx < samplesI.size();
  }

  // Analytic RF signal
  void getRfIQ(double timeS, double& outI, double& outQ) const override {
    if (samplesI.empty()) {
      outI = outQ = 0.0;
      return;
    }

    double sampleIdxF = timeS * sampleRateHz;
    size_t totalSamples = samplesI.size();

    if (loop) {
      sampleIdxF = std::fmod(sampleIdxF, static_cast<double>(totalSamples));
      if (sampleIdxF < 0) {
        sampleIdxF += totalSamples;
      }
    } else if (sampleIdxF >= totalSamples || sampleIdxF < 0) {
      outI = outQ = 0.0;
      return;
    }

    size_t idx0 = static_cast<size_t>(sampleIdxF);
    size_t idx1 = (idx0 + 1) % totalSamples;
    double frac = sampleIdxF - idx0;

    double iVal = samplesI[idx0] * (1.0 - frac) + samplesI[idx1] * frac;
    double qVal = samplesQ[idx0] * (1.0 - frac) + samplesQ[idx1] * frac;

    if (swapIQ) {
      std::swap(iVal, qVal);
    }

    if (centerFreqHz > 0) {
      // Use stateful phasor to avoid expensive sin/cos in inner loops
      // if we are being called sequentially.
      double cp, sp;
      if (std::abs(timeS - lastTimeS - lastDeltaS) < 1e-11 && lastTimeS >= 0) {
        // Sequential call: Rotate previous phasor
        cp = lastCos * cosD - lastSin * sinD;
        sp = lastSin * cosD + lastCos * sinD;
      } else {
        // Non-sequential: Full sin/cos and reset phasor
        double phase = 2.0 * M_PI * centerFreqHz * timeS;
        cp = std::cos(phase);
        sp = std::sin(phase);
        
        // Update rotation delta for future sequential calls
        // Note: we can't know the delta until the second call, so we'll 
        // just compute it based on a guess or wait for the next call.
        // Actually, let's just use the current timeS to reset.
        lastDeltaS = 0; // Will be set on next call
      }
      
      if (lastDeltaS == 0 && lastTimeS >= 0 && timeS > lastTimeS) {
          lastDeltaS = timeS - lastTimeS;
          double dPhi = 2.0 * M_PI * centerFreqHz * lastDeltaS;
          cosD = std::cos(dPhi);
          sinD = std::sin(dPhi);
      }

      lastTimeS = timeS;
      lastCos = cp;
      lastSin = sp;

      outI = (iVal * cp - qVal * sp) * amplitudeV;
      outQ = (iVal * sp + qVal * cp) * amplitudeV;
    } else {
      outI = iVal * amplitudeV;
      outQ = qVal * amplitudeV;
    }
  }

  // Get duration of loaded capture
  [[nodiscard]] double getDuration() const {
    return samplesI.size() * samplePeriodS;
  }

  // Get sample count
  [[nodiscard]] size_t getSampleCount() const {
    return samplesI.size();
  }

  // Get sample rate
  [[nodiscard]] double getSampleRate() const {
    return sampleRateHz;
  }

  // AntennaStimulus Overrides
  [[nodiscard]] double carrierFrequency() const override { return centerFreqHz; }
  [[nodiscard]] bool isBroadband() const override { return false; }

private:
  std::vector<double> samplesI;
  std::vector<double> samplesQ;
  double sampleRateHz = 0;
  double samplePeriodS = 0;
  double centerFreqHz = 0;
  double amplitudeV = 1.0;
  bool loop = false;
  bool swapIQ = false;
  std::string path;

  // Phasor state for upconversion performance
  mutable double lastTimeS = -1.0;
  mutable double lastDeltaS = 0.0;
  mutable double lastCos = 1.0;
  mutable double lastSin = 0.0;
  mutable double cosD = 1.0;
  mutable double sinD = 0.0;

  void loadFloat32IQ(std::ifstream& file, size_t fileSize) {
    size_t numSamples = fileSize / (2 * sizeof(float));
    samplesI.resize(numSamples);
    samplesQ.resize(numSamples);

    for (size_t i = 0; i < numSamples; ++i) {
      float iq[2];
      file.read(reinterpret_cast<char*>(iq), sizeof(iq));
      samplesI[i] = iq[0];
      samplesQ[i] = iq[1];
    }
  }

  void loadFloat64IQ(std::ifstream& file, size_t fileSize) {
    size_t numSamples = fileSize / (2 * sizeof(double));
    samplesI.resize(numSamples);
    samplesQ.resize(numSamples);

    for (size_t i = 0; i < numSamples; ++i) {
      double iq[2];
      file.read(reinterpret_cast<char*>(iq), sizeof(iq));
      samplesI[i] = iq[0];
      samplesQ[i] = iq[1];
    }
  }

  void loadInt16IQ(std::ifstream& file, size_t fileSize) {
    size_t numSamples = fileSize / (2 * sizeof(int16_t));
    samplesI.resize(numSamples);
    samplesQ.resize(numSamples);

    constexpr double scale = 1.0 / 32768.0;
    for (size_t i = 0; i < numSamples; ++i) {
      int16_t iq[2];
      file.read(reinterpret_cast<char*>(iq), sizeof(iq));
      samplesI[i] = iq[0] * scale;
      samplesQ[i] = iq[1] * scale;
    }
  }

  void loadInt8IQ(std::ifstream& file, size_t fileSize) {
    size_t numSamples = fileSize / 2;
    samplesI.resize(numSamples);
    samplesQ.resize(numSamples);

    constexpr double scale = 1.0 / 128.0;
    for (size_t i = 0; i < numSamples; ++i) {
      int8_t iq[2];
      file.read(reinterpret_cast<char*>(iq), sizeof(iq));
      samplesI[i] = iq[0] * scale;
      samplesQ[i] = iq[1] * scale;
    }
  }

  void loadUint8IQ(std::ifstream& file, size_t fileSize) {
    size_t numSamples = fileSize / 2;
    samplesI.resize(numSamples);
    samplesQ.resize(numSamples);

    constexpr double scale = 1.0 / 128.0;
    for (size_t i = 0; i < numSamples; ++i) {
      uint8_t iq[2];
      file.read(reinterpret_cast<char*>(iq), sizeof(iq));
      // Offset binary: 128 = 0
      samplesI[i] = (static_cast<int>(iq[0]) - 128) * scale;
      samplesQ[i] = (static_cast<int>(iq[1]) - 128) * scale;
    }
  }
};

} // namespace nexrx
