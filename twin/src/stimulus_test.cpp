// NexRx Digital Twin - Stimulus Source Test
//
// Validates the antenna stimulus generators:
//   - ToneGenerator (single/multi-tone CW)
//   - NoiseGenerator (thermal noise)
//   - CompositeStimulus (signal + noise)
//
// Copyright 2026 NexRx Project - MIT License

#include "stimulus/AntennaStimulus.hpp"
#include "stimulus/ToneGenerator.hpp"
#include "stimulus/NoiseGenerator.hpp"
#include "stimulus/RfCapturePlayer.hpp"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <numeric>

using namespace nexrx;

// Calculate RMS of a sample vector
double calculateRms(const std::vector<double>& samples) {
    double sum_sq = 0;
    for (double s : samples) {
        sum_sq += s * s;
    }
    return std::sqrt(sum_sq / samples.size());
}

// Calculate peak value
double calculatePeak(const std::vector<double>& samples) {
    double peak = 0;
    for (double s : samples) {
        peak = std::max(peak, std::abs(s));
    }
    return peak;
}

// Estimate frequency via zero-crossing rate
double estimateFrequency(const std::vector<double>& samples, double sample_rate) {
    int crossings = 0;
    for (size_t i = 1; i < samples.size(); ++i) {
        if ((samples[i-1] >= 0 && samples[i] < 0) ||
            (samples[i-1] < 0 && samples[i] >= 0)) {
            crossings++;
        }
    }
    // Each full cycle has 2 zero crossings
    return (crossings / 2.0) * sample_rate / samples.size();
}

void testToneGenerator() {
    std::cout << "\n=== ToneGenerator Tests ===" << std::endl;

    // Test 1: Single 14.01 MHz tone, 100mV peak
    {
        auto tone = ToneGenerator::cw(14.01e6, 0.1);
        std::cout << "\n1. " << tone.description() << std::endl;

        // Sample at 100 MHz (well above Nyquist for 14 MHz)
        constexpr double sample_rate = 100e6;
        constexpr int num_samples = 10000;
        std::vector<double> samples(num_samples);

        for (int i = 0; i < num_samples; ++i) {
            double t = i / sample_rate;
            samples[i] = tone.getSample(t);
        }

        double rms = calculateRms(samples);
        double peak = calculatePeak(samples);
        double freq = estimateFrequency(samples, sample_rate);

        std::cout << "   Peak: " << peak * 1000 << " mV (expected 100 mV)" << std::endl;
        std::cout << "   RMS:  " << rms * 1000 << " mV (expected 70.7 mV)" << std::endl;
        std::cout << "   Freq: " << freq / 1e6 << " MHz (expected 14.01 MHz)" << std::endl;
    }

    // Test 2: Two-tone IMD test signal
    {
        auto twoTone = ToneGenerator::twoTone(14.0e6, 1000.0, 0.05);
        std::cout << "\n2. " << twoTone.description() << std::endl;

        // The two tones should create a beat pattern
        constexpr double sample_rate = 100e6;
        constexpr int num_samples = 100000;  // 1ms at 100MHz
        std::vector<double> samples(num_samples);

        for (int i = 0; i < num_samples; ++i) {
            double t = i / sample_rate;
            samples[i] = twoTone.getSample(t);
        }

        double rms = calculateRms(samples);
        double peak = calculatePeak(samples);

        std::cout << "   Peak: " << peak * 1000 << " mV (expected ~100 mV when in phase)" << std::endl;
        std::cout << "   RMS:  " << rms * 1000 << " mV" << std::endl;
        std::cout << "   Beat freq: 1 kHz (spacing between tones)" << std::endl;
    }

    // Test 3: AM signal
    {
        auto am = ToneGenerator::amSignal(14.0e6, 1000.0, 0.1, 0.5);
        std::cout << "\n3. " << am.description() << std::endl;
        std::cout << "   AM carrier 14 MHz, 1kHz modulation, 50% depth" << std::endl;
        std::cout << "   Tones: " << am.toneCount() << " (carrier + 2 sidebands)" << std::endl;
    }

    // Test 4: USB/LSB SSB
    {
        auto usb = ToneGenerator::usbSignal(14.0e6, 1000.0, 0.1);
        auto lsb = ToneGenerator::lsbSignal(14.0e6, 1000.0, 0.1);
        std::cout << "\n4. SSB signals:" << std::endl;
        std::cout << "   USB: " << usb.description() << " (14.001 MHz)" << std::endl;
        std::cout << "   LSB: " << lsb.description() << " (13.999 MHz)" << std::endl;
    }
}

void testNoiseGenerator() {
    std::cout << "\n=== NoiseGenerator Tests ===" << std::endl;

    // Test 1: Direct RMS specification
    {
        auto noise = NoiseGenerator(1e-6, 12345);  // 1 uV RMS, fixed seed
        std::cout << "\n1. " << noise.description() << std::endl;

        constexpr int num_samples = 100000;
        std::vector<double> samples(num_samples);

        for (int i = 0; i < num_samples; ++i) {
            samples[i] = noise.getSample(i * 1e-6);  // Time doesn't matter for white noise
        }

        double rms = calculateRms(samples);
        std::cout << "   Measured RMS: " << rms * 1e6 << " uV (expected 1.0 uV)" << std::endl;
    }

    // Test 2: Thermal noise at room temperature
    {
        // 290K, 3kHz bandwidth, 50 ohm
        auto thermal = NoiseGenerator::thermal(290.0, 3000.0, 50.0, 12345);
        std::cout << "\n2. Thermal noise (290K, 3kHz BW, 50 ohm)" << std::endl;

        // Expected: V_rms = sqrt(4 * k * T * B * R)
        //         = sqrt(4 * 1.38e-23 * 290 * 3000 * 50)
        //         = sqrt(2.4e-14) = 155 nV
        constexpr int num_samples = 100000;
        std::vector<double> samples(num_samples);
        for (int i = 0; i < num_samples; ++i) {
            samples[i] = thermal.getSample(i * 1e-6);
        }
        double rms = calculateRms(samples);
        std::cout << "   Measured RMS: " << rms * 1e9 << " nV (expected ~155 nV)" << std::endl;
    }

    // Test 3: Noise figure specification
    {
        // 10 dB NF system, 3kHz bandwidth
        auto nfNoise = NoiseGenerator::fromNoiseFigure(10.0, 3000.0, 50.0, 290.0, 12345);
        std::cout << "\n3. System noise (NF=10dB, 3kHz BW)" << std::endl;

        constexpr int num_samples = 100000;
        std::vector<double> samples(num_samples);
        for (int i = 0; i < num_samples; ++i) {
            samples[i] = nfNoise.getSample(i * 1e-6);
        }
        double rms = calculateRms(samples);
        std::cout << "   Measured RMS: " << rms * 1e9 << " nV" << std::endl;
    }

    // Test 4: S-meter levels
    {
        std::cout << "\n4. S-meter reference levels (50 ohm):" << std::endl;
        for (int s = 1; s <= 9; s += 2) {
            auto sNoise = NoiseGenerator::sLevel(s, 12345);
            std::cout << "   S" << s << ": " << sNoise.rmsVoltage() * 1e6 << " uV" << std::endl;
        }
        // S9 should be 50 uV
    }
}

void testCompositeStimulus() {
    std::cout << "\n=== CompositeStimulus Tests ===" << std::endl;

    // Signal + Noise test
    auto signal = std::make_shared<ToneGenerator>(14.01e6, 0.001);  // 1mV signal
    auto noise = std::make_shared<NoiseGenerator>(100e-6, 12345);   // 100uV noise

    CompositeStimulus composite;
    composite.addSource(signal);
    composite.addSource(noise);

    std::cout << "\n" << composite.description() << std::endl;
    std::cout << "Signal: 1mV @ 14.01 MHz" << std::endl;
    std::cout << "Noise: 100uV RMS" << std::endl;
    std::cout << "Expected SNR: 20 dB" << std::endl;

    // Sample and measure
    constexpr double sample_rate = 100e6;
    constexpr int num_samples = 10000;
    std::vector<double> samples(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        double t = i / sample_rate;
        samples[i] = composite.getSample(t);
    }

    double rms = calculateRms(samples);
    double peak = calculatePeak(samples);
    std::cout << "Measured RMS: " << rms * 1000 << " mV" << std::endl;
    std::cout << "Measured Peak: " << peak * 1000 << " mV" << std::endl;
}

void testRfCapturePlayer() {
    std::cout << "\n=== RfCapturePlayer Tests ===" << std::endl;

    // Create synthetic I/Q data: 10kHz baseband tone
    std::vector<float> i_samples, q_samples;
    constexpr double baseband_freq = 10000.0;  // 10 kHz
    constexpr double sample_rate = 96000.0;    // 96 kHz
    constexpr int num_samples = 960;           // 10ms of data

    for (int i = 0; i < num_samples; ++i) {
        double t = i / sample_rate;
        double phase = 2.0 * M_PI * baseband_freq * t;
        i_samples.push_back(static_cast<float>(std::cos(phase)));
        q_samples.push_back(static_cast<float>(std::sin(phase)));
    }

    RfCapturePlayer player;
    player.loadData(i_samples, q_samples, sample_rate);
    player.setCenterFrequency(14.0e6);  // Upconvert to 14 MHz
    player.setAmplitude(0.1);           // 100mV output
    player.setLooping(true);

    std::cout << "\n" << player.description() << std::endl;
    std::cout << "Baseband: 10 kHz I/Q" << std::endl;
    std::cout << "Output: 14.010 MHz RF" << std::endl;

    // Sample the output at high rate
    constexpr double output_rate = 100e6;
    constexpr int output_samples = 10000;
    std::vector<double> samples(output_samples);

    for (int i = 0; i < output_samples; ++i) {
        double t = i / output_rate;
        samples[i] = player.getSample(t);
    }

    double rms = calculateRms(samples);
    double peak = calculatePeak(samples);
    double freq = estimateFrequency(samples, output_rate);

    std::cout << "Measured Peak: " << peak * 1000 << " mV" << std::endl;
    std::cout << "Measured RMS: " << rms * 1000 << " mV" << std::endl;
    std::cout << "Measured Freq: " << freq / 1e6 << " MHz" << std::endl;
}

int main() {
    std::cout << "=== NexRx Digital Twin Stimulus Test ===" << std::endl;

    testToneGenerator();
    testNoiseGenerator();
    testCompositeStimulus();
    testRfCapturePlayer();

    std::cout << "\n=== All Tests Complete ===" << std::endl;
    return 0;
}
