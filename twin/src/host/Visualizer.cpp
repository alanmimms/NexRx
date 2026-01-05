// NexRx Digital Twin - Terminal Visualizer Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "Visualizer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace nexrx {

Visualizer::Visualizer()
    : spectrum_(SPECTRUM_BINS)
{
    constellation_.fill(Complex{0, 0});
    lastUpdate_ = std::chrono::steady_clock::now();
}

void Visualizer::update(const std::vector<Complex>& samples) {
    for (const auto& s : samples) {
        update(s);
    }
}

void Visualizer::update(const Complex& sample) {
    // Update spectrum analyzer
    spectrum_.addSample(sample);

    // Update level tracking
    double mag = std::abs(sample);
    updateLevel(mag);

    // Update constellation
    constellation_[constellationPos_] = sample;
    constellationPos_ = (constellationPos_ + 1) % CONSTELLATION_SIZE;

    ++totalSamples_;
}

void Visualizer::updateLevel(double mag) {
    peakLevel_ = std::max(peakLevel_, mag);
    sumMagSq_ += mag * mag;
    ++sampleCount_;

    // Update RMS every 96 samples
    if (sampleCount_ >= 96) {
        rmsLevel_ = std::sqrt(sumMagSq_ / sampleCount_);
        sumMagSq_ = 0;
        sampleCount_ = 0;
        // Decay peak
        peakLevel_ *= 0.95;
    }
}

double Visualizer::levelDbm() const {
    if (rmsLevel_ <= 0) return -120.0;

    // V_rms to power in 50 ohms: P = V^2 / 50
    // dBm = 10 * log10(P / 1mW) = 10 * log10(V^2 / 50 / 0.001)
    //     = 10 * log10(V^2 / 0.05) = 20 * log10(V) + 13
    return 20.0 * std::log10(rmsLevel_) + 13.0;
}

int Visualizer::sUnits() const {
    double dbm = levelDbm();

    // S9 = -73 dBm, each S-unit = 6 dB
    int s = static_cast<int>(std::round((dbm + 73.0) / 6.0)) + 9;

    return std::clamp(s, 0, 20);  // S0 to S9+60
}

std::string Visualizer::renderLevelMeter() const {
    std::ostringstream oss;

    int s = sUnits();
    double dbm = levelDbm();

    // S-meter label
    if (s <= 9) {
        oss << "S" << s << " ";
    } else {
        oss << "S9+" << (s - 9) * 10 << " ";
    }

    // Level bar
    // Map -120 to -30 dBm to 0-40 chars
    int barLen = static_cast<int>((dbm + 120.0) / 90.0 * 40.0);
    barLen = std::clamp(barLen, 0, 40);

    oss << term::levelColor(dbm) << "[";

    for (int i = 0; i < 40; ++i) {
        if (i < barLen) {
            oss << "\u2588";  // Full block
        } else {
            oss << "\u2591";  // Light shade
        }
    }

    oss << "] " << term::RESET;
    oss << std::fixed << std::setprecision(1) << dbm << " dBm";

    return oss.str();
}

std::string Visualizer::renderSpectrum() const {
    std::ostringstream oss;

    const auto& spec = spectrum_.spectrum();

    // Find range
    double maxDb = -120, minDb = 0;
    for (double db : spec) {
        maxDb = std::max(maxDb, db);
        minDb = std::min(minDb, db);
    }

    // Normalize to 0-8 for display height
    auto normalize = [&](double db) -> int {
        double range = maxDb - minDb;
        if (range < 1) range = 1;
        return static_cast<int>((db - minDb) / range * 8.0);
    };

    // Render 8 rows, top to bottom
    const char* blocks[] = {" ", "\u2581", "\u2582", "\u2583",
                            "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"};

    oss << "Spectrum:\n";

    for (int row = 7; row >= 0; --row) {
        oss << "  ";
        for (size_t bin = 0; bin < spec.size(); ++bin) {
            int height = normalize(spec[bin]);
            if (height > row) {
                oss << term::GREEN << "\u2588" << term::RESET;
            } else {
                oss << " ";
            }
        }
        oss << "\n";
    }

    // Frequency axis
    oss << "  ";
    for (size_t i = 0; i < spec.size(); ++i) {
        if (i == 0) oss << "-";
        else if (i == spec.size() / 2) oss << "0";
        else if (i == spec.size() - 1) oss << "+";
        else oss << "-";
    }
    oss << "\n";

    return oss.str();
}

std::string Visualizer::renderConstellation() const {
    std::ostringstream oss;

    // 16x8 ASCII constellation display
    constexpr int WIDTH = 16;
    constexpr int HEIGHT = 8;
    std::array<std::array<char, WIDTH>, HEIGHT> grid;

    // Fill with dots
    for (auto& row : grid) {
        row.fill('.');
    }

    // Mark center
    grid[HEIGHT/2][WIDTH/2] = '+';

    // Plot samples
    double maxMag = 0;
    for (const auto& s : constellation_) {
        maxMag = std::max(maxMag, std::abs(s));
    }
    if (maxMag < 1e-9) maxMag = 1.0;

    for (const auto& s : constellation_) {
        // Normalize to grid coordinates
        int x = static_cast<int>((s.real() / maxMag + 1.0) * (WIDTH - 1) / 2.0);
        int y = static_cast<int>((1.0 - s.imag() / maxMag) * (HEIGHT - 1) / 2.0);

        x = std::clamp(x, 0, WIDTH - 1);
        y = std::clamp(y, 0, HEIGHT - 1);

        grid[y][x] = '*';
    }

    oss << "Constellation:\n";
    for (int y = 0; y < HEIGHT; ++y) {
        oss << "  |";
        for (int x = 0; x < WIDTH; ++x) {
            char c = grid[y][x];
            if (c == '*') {
                oss << term::CYAN << c << term::RESET;
            } else if (c == '+') {
                oss << term::YELLOW << c << term::RESET;
            } else {
                oss << c;
            }
        }
        oss << "|\n";
    }

    return oss.str();
}

std::string Visualizer::renderStats() const {
    std::ostringstream oss;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - lastUpdate_).count();

    oss << "Statistics:\n";
    oss << "  RMS Level: " << std::fixed << std::setprecision(3)
        << rmsLevel_ * 1000.0 << " mV\n";
    oss << "  Peak Level: " << peakLevel_ * 1000.0 << " mV\n";
    oss << "  S-Meter: S" << sUnits() << "\n";
    oss << "  Level: " << std::setprecision(1) << levelDbm() << " dBm\n";
    oss << "  Samples: " << totalSamples_ << "\n";

    return oss.str();
}

std::string Visualizer::render() const {
    std::ostringstream oss;

    oss << term::BOLD << "=== NexRx Digital Twin - Host Visualizer ===" << term::RESET << "\n\n";
    oss << renderLevelMeter() << "\n\n";
    oss << renderSpectrum() << "\n";
    oss << renderConstellation() << "\n";
    oss << renderStats();

    return oss.str();
}

void Visualizer::display() const {
    std::cout << term::CLEAR;
    std::cout << render();
    std::cout << std::flush;
}

void Visualizer::reset() {
    spectrum_.reset();
    peakLevel_ = 0;
    rmsLevel_ = 0;
    sumMagSq_ = 0;
    sampleCount_ = 0;
    constellation_.fill(Complex{0, 0});
    constellationPos_ = 0;
    totalSamples_ = 0;
    lastUpdate_ = std::chrono::steady_clock::now();
}

} // namespace nexrx
