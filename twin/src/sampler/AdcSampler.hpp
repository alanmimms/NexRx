// NexRx Digital Twin - ADC Sampler
//
// Samples Xyce I/Q output nodes at 96kHz ADC rate.
// Converts analog voltages to 24-bit signed digital values.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "transport/IQFrame.hpp"
#include "xyce/XyceWrapper.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace nexrx {

//======================================================================
// ADC Configuration
//======================================================================
struct AdcConfig {
    // Sample rate
    static constexpr double SAMPLE_RATE_HZ = 96000.0;
    static constexpr double SAMPLE_PERIOD_S = 1.0 / SAMPLE_RATE_HZ;
    static constexpr uint64_t SAMPLE_PERIOD_NS = 10416;  // 1e9 / 96000

    // ADC specifications (matching real hardware)
    static constexpr double VREF = 3.3;                   // ADC reference voltage
    static constexpr double VCM = 1.65;                   // Common-mode voltage (mid-rail)
    static constexpr double FULL_SCALE = 1.65;            // +/- 1.65V around VCM
    static constexpr int32_t ADC_MAX = 8388607;           // 2^23 - 1 (24-bit signed)
    static constexpr int32_t ADC_MIN = -8388608;          // -2^23

    // Node names in the netlist for I/Q outputs
    // These map to the QSD outputs in nexrx_rx.cir
    std::array<std::string, 3> i_nodes = {"Q0_I", "Q1_I", "Q2_I"};
    std::array<std::string, 3> q_nodes = {"Q0_Q", "Q1_Q", "Q2_Q"};
};

//======================================================================
// ADC Sampler
//
// Samples the 6 I/Q channels from Xyce at 96kHz rate.
// Maintains sample timing and sequence numbering.
//
// Usage:
//   AdcSampler sampler(xyceWrapper, config);
//   while (running) {
//       xyce.stepTo(sampler.nextSampleTime());
//       if (auto frame = sampler.sample()) {
//           transport.write(*frame);
//       }
//   }
//======================================================================
class AdcSampler {
public:
    using FrameCallback = std::function<void(const IQFrame&)>;

    explicit AdcSampler(NexRx::Twin::XyceWrapper& xyce, const AdcConfig& config = {});

    // Get the time of the next sample in seconds
    [[nodiscard]] double nextSampleTime() const;

    // Get the time of the next sample in nanoseconds
    [[nodiscard]] uint64_t nextSampleTimeNs() const;

    // Check if current Xyce time is at or past next sample time
    [[nodiscard]] bool isSampleDue() const;

    // Take a sample from Xyce, returns frame if successful
    std::optional<IQFrame> sample();

    // Take a sample and invoke callback
    bool sampleWithCallback(const FrameCallback& callback);

    // Reset sampler state (e.g., for new simulation)
    void reset();

    // Get current sample count
    [[nodiscard]] uint64_t sampleCount() const { return sampleCount_; }

    // Get current sequence number (wraps at 2^32)
    [[nodiscard]] uint32_t sequence() const { return sequence_; }

    // Statistics
    [[nodiscard]] uint64_t missedSamples() const { return missedSamples_; }
    [[nodiscard]] uint64_t errorCount() const { return errorCount_; }

private:
    // Convert voltage to 24-bit ADC value
    static int32_t voltageToAdc(double voltage, double vcm = AdcConfig::VCM);

    // Sample a single I/Q pair
    std::optional<IQSample> sampleChannel(size_t channelIndex);

    NexRx::Twin::XyceWrapper& xyce_;
    AdcConfig config_;

    uint64_t nextSampleTimeNs_ = 0;
    uint64_t sampleCount_ = 0;
    uint32_t sequence_ = 0;

    // Error tracking
    uint64_t missedSamples_ = 0;
    uint64_t errorCount_ = 0;
};

} // namespace nexrx
