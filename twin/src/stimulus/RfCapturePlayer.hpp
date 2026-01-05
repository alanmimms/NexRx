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
// center_frequency to 0.
//======================================================================
class RfCapturePlayer : public AntennaStimulus {
public:
    RfCapturePlayer() = default;

    // Load I/Q data from file
    bool loadFile(const std::string& path, IQFormat format, double sample_rate_hz) {
        sample_rate_hz_ = sample_rate_hz;
        sample_period_s_ = 1.0 / sample_rate_hz;

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        samples_i_.clear();
        samples_q_.clear();

        switch (format) {
            case IQFormat::FLOAT32_IQ:
                loadFloat32IQ(file, file_size);
                break;
            case IQFormat::FLOAT64_IQ:
                loadFloat64IQ(file, file_size);
                break;
            case IQFormat::INT16_IQ:
                loadInt16IQ(file, file_size);
                break;
            case IQFormat::INT8_IQ:
                loadInt8IQ(file, file_size);
                break;
            case IQFormat::UINT8_IQ:
                loadUint8IQ(file, file_size);
                break;
            case IQFormat::COMPLEX_FLOAT32:
                loadFloat32IQ(file, file_size);  // Same layout
                break;
        }

        path_ = path;
        return !samples_i_.empty();
    }

    // Load from raw float vectors (for programmatic use)
    void loadData(const std::vector<float>& i_samples,
                  const std::vector<float>& q_samples,
                  double sample_rate_hz) {
        samples_i_.assign(i_samples.begin(), i_samples.end());
        samples_q_.assign(q_samples.begin(), q_samples.end());
        sample_rate_hz_ = sample_rate_hz;
        sample_period_s_ = 1.0 / sample_rate_hz;
        path_ = "(memory)";
    }

    // Set center frequency for upconversion (0 = baseband playback)
    void setCenterFrequency(double freq_hz) {
        center_freq_hz_ = freq_hz;
    }

    // Set amplitude scaling
    void setAmplitude(double amplitude_v) {
        amplitude_v_ = amplitude_v;
    }

    // Enable/disable looping
    void setLooping(bool loop) {
        loop_ = loop;
    }

    [[nodiscard]] double getSample(double time_s) const override {
        if (samples_i_.empty()) {
            return 0.0;
        }

        // Calculate sample index
        double sample_idx_f = time_s * sample_rate_hz_;
        size_t total_samples = samples_i_.size();

        if (loop_) {
            sample_idx_f = std::fmod(sample_idx_f, static_cast<double>(total_samples));
            if (sample_idx_f < 0) sample_idx_f += total_samples;
        } else if (sample_idx_f >= total_samples || sample_idx_f < 0) {
            return 0.0;
        }

        // Linear interpolation between samples
        size_t idx0 = static_cast<size_t>(sample_idx_f);
        size_t idx1 = (idx0 + 1) % total_samples;
        double frac = sample_idx_f - idx0;

        double i_val = samples_i_[idx0] * (1.0 - frac) + samples_i_[idx1] * frac;
        double q_val = samples_q_[idx0] * (1.0 - frac) + samples_q_[idx1] * frac;

        // Upconvert to RF if center frequency is set
        double output;
        if (center_freq_hz_ > 0) {
            double phase = 2.0 * M_PI * center_freq_hz_ * time_s;
            output = i_val * std::cos(phase) - q_val * std::sin(phase);
        } else {
            // Baseband: just return I component (or could return magnitude)
            output = i_val;
        }

        return output * amplitude_v_;
    }

    [[nodiscard]] std::string description() const override {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << "RfCapture(" << path_ << ", "
            << samples_i_.size() << " samples, "
            << sample_rate_hz_ / 1e6 << " MHz rate";
        if (center_freq_hz_ > 0) {
            oss << ", fc=" << center_freq_hz_ / 1e6 << " MHz";
        }
        oss << ")";
        return oss.str();
    }

    void reset() override {
        // Nothing to reset for stateless playback
    }

    [[nodiscard]] bool hasMore(double time_s) const override {
        if (loop_) return true;
        double sample_idx = time_s * sample_rate_hz_;
        return sample_idx < samples_i_.size();
    }

    // Get duration of loaded capture
    [[nodiscard]] double duration() const {
        return samples_i_.size() * sample_period_s_;
    }

    // Get sample count
    [[nodiscard]] size_t sampleCount() const {
        return samples_i_.size();
    }

    // Get sample rate
    [[nodiscard]] double sampleRate() const {
        return sample_rate_hz_;
    }

private:
    std::vector<double> samples_i_;
    std::vector<double> samples_q_;
    double sample_rate_hz_ = 0;
    double sample_period_s_ = 0;
    double center_freq_hz_ = 0;
    double amplitude_v_ = 1.0;
    bool loop_ = false;
    std::string path_;

    void loadFloat32IQ(std::ifstream& file, size_t file_size) {
        size_t num_samples = file_size / (2 * sizeof(float));
        samples_i_.resize(num_samples);
        samples_q_.resize(num_samples);

        for (size_t i = 0; i < num_samples; ++i) {
            float iq[2];
            file.read(reinterpret_cast<char*>(iq), sizeof(iq));
            samples_i_[i] = iq[0];
            samples_q_[i] = iq[1];
        }
    }

    void loadFloat64IQ(std::ifstream& file, size_t file_size) {
        size_t num_samples = file_size / (2 * sizeof(double));
        samples_i_.resize(num_samples);
        samples_q_.resize(num_samples);

        for (size_t i = 0; i < num_samples; ++i) {
            double iq[2];
            file.read(reinterpret_cast<char*>(iq), sizeof(iq));
            samples_i_[i] = iq[0];
            samples_q_[i] = iq[1];
        }
    }

    void loadInt16IQ(std::ifstream& file, size_t file_size) {
        size_t num_samples = file_size / (2 * sizeof(int16_t));
        samples_i_.resize(num_samples);
        samples_q_.resize(num_samples);

        constexpr double scale = 1.0 / 32768.0;
        for (size_t i = 0; i < num_samples; ++i) {
            int16_t iq[2];
            file.read(reinterpret_cast<char*>(iq), sizeof(iq));
            samples_i_[i] = iq[0] * scale;
            samples_q_[i] = iq[1] * scale;
        }
    }

    void loadInt8IQ(std::ifstream& file, size_t file_size) {
        size_t num_samples = file_size / 2;
        samples_i_.resize(num_samples);
        samples_q_.resize(num_samples);

        constexpr double scale = 1.0 / 128.0;
        for (size_t i = 0; i < num_samples; ++i) {
            int8_t iq[2];
            file.read(reinterpret_cast<char*>(iq), sizeof(iq));
            samples_i_[i] = iq[0] * scale;
            samples_q_[i] = iq[1] * scale;
        }
    }

    void loadUint8IQ(std::ifstream& file, size_t file_size) {
        size_t num_samples = file_size / 2;
        samples_i_.resize(num_samples);
        samples_q_.resize(num_samples);

        constexpr double scale = 1.0 / 128.0;
        for (size_t i = 0; i < num_samples; ++i) {
            uint8_t iq[2];
            file.read(reinterpret_cast<char*>(iq), sizeof(iq));
            // Offset binary: 128 = 0
            samples_i_[i] = (static_cast<int>(iq[0]) - 128) * scale;
            samples_q_[i] = (static_cast<int>(iq[1]) - 128) * scale;
        }
    }
};

} // namespace nexrx
