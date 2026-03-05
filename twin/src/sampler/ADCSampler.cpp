// NexRx Digital Twin - ADC Sampler Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "ADCSampler.hpp"

#include <algorithm>
#include <cmath>

namespace nexrx {

ADCSampler::ADCSampler(XyceWrapper& xyceIn, const ADCConfig& configIn)
  : xyce(xyceIn)
  , config(configIn) {
}

double ADCSampler::getNextSampleTime() const {
  return static_cast<double>(nextSampleTimeNS) * 1e-9;
}

uint64_t ADCSampler::getNextSampleTimeNS() const {
  return nextSampleTimeNS;
}

bool ADCSampler::isSampleDue() const {
  auto currentTime = xyce.getCurrentTime();
  if (!currentTime) {
    return false;
  }

  double currentTimeNS = *currentTime * 1e9;
  return currentTimeNS >= static_cast<double>(nextSampleTimeNS);
}

int32_t ADCSampler::voltageToADC(double voltage, double vcm) {
  // Remove DC bias (common-mode voltage)
  double vDiff = voltage - vcm;

  // Scale to ADC range: +/- 1.65V -> +/- 2^23
  double scaled = vDiff / ADCConfig::FULL_SCALE * static_cast<double>(ADCConfig::ADC_MAX);

  // Clamp to valid range
  scaled = std::clamp(scaled,
                      static_cast<double>(ADCConfig::ADC_MIN),
                      static_cast<double>(ADCConfig::ADC_MAX));

  return static_cast<int32_t>(std::round(scaled));
}

std::optional<IQSample> ADCSampler::sampleChannel(size_t channelIndex) {
  if (channelIndex >= 3) {
    return std::nullopt;
  }

  // Get I and Q voltages for this channel
  auto vI = xyce.getNodeVoltage(config.iNodes[channelIndex]);
  auto vQ = xyce.getNodeVoltage(config.qNodes[channelIndex]);

  if (!vI || !vQ) {
    ++errorCount;
    return std::nullopt;
  }

  // Convert to ADC values
  IQSample sampleVal;
  sampleVal.i = voltageToADC(*vI);
  sampleVal.q = voltageToADC(*vQ);

  return sampleVal;
}

std::optional<IQFrame> ADCSampler::sample() {
  // Check if Xyce has reached the sample time
  auto currentTime = xyce.getCurrentTime();
  if (!currentTime) {
    return std::nullopt;
  }

  double currentTimeNS = *currentTime * 1e9;

  // Check for missed samples
  while (static_cast<double>(nextSampleTimeNS) + ADCConfig::SAMPLE_PERIOD_NS < currentTimeNS) {
    // We missed a sample
    ++missedSamples;
    nextSampleTimeNS += ADCConfig::SAMPLE_PERIOD_NS;
    ++sequence;
  }

  if (currentTimeNS < static_cast<double>(nextSampleTimeNS)) {
    // Not time for a sample yet
    return std::nullopt;
  }

  // Create frame
  IQFrame frame;
  frame.timestampNS = nextSampleTimeNS;
  frame.sequence = sequence;
  frame.flags = 0;

  // Sample all three QSD channels
  bool allValid = true;
  for (size_t i = 0; i < 3; ++i) {
    auto sampleVal = sampleChannel(i);
    if (sampleVal) {
      frame.qsd[i] = *sampleVal;
    } else {
      frame.qsd[i] = IQSample{0, 0};
      allValid = false;
    }
  }

  // Update timing for next sample
  nextSampleTimeNS += ADCConfig::SAMPLE_PERIOD_NS;
  ++sampleCount;
  ++sequence;

  if (!allValid) {
    frame.flags |= 0x01;  // Mark frame as having errors
  }

  return frame;
}

bool ADCSampler::sampleWithCallback(const FrameCallback& callback) {
  auto frame = sample();
  if (frame && callback) {
    callback(*frame);
    return true;
  }
  return false;
}

void ADCSampler::reset() {
  nextSampleTimeNS = 0;
  sampleCount = 0;
  sequence = 0;
  missedSamples = 0;
  errorCount = 0;
}

} // namespace nexrx
