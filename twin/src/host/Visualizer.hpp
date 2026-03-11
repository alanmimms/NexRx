// NexRx Digital Twin - Terminal Visualizer
//
// Simple terminal-based visualization for I/Q signals.
// Displays signal levels, spectrum, and constellation.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "DSPPipeline.hpp"
#include "transport/IQFrame.hpp"

#include <array>
#include <chrono>
#include <complex>
#include <string>
#include <vector>

namespace nexrx {

//======================================================================
// Terminal Visualizer
//
// Provides text-based visualization of I/Q signals:
// - Signal level meter (S-meter style)
// - Simple spectrum display
// - Statistics summary
//======================================================================
class Visualizer {
public:
    // Display width for visualizations
    static constexpr size_t DISPLAY_WIDTH = 60;
    static constexpr size_t SPECTRUM_BINS = 32;

    Visualizer();

    // Update with new samples
    void update(const std::vector<Complex>& samples);
    void update(const Complex& sample);

    // Render signal level bar
    // Returns string like: "S7 [████████████░░░░░░░░░░░░░░░░] -85 dBm"
    [[nodiscard]] std::string renderLevelMeter() const;

    // Render spectrum as horizontal bars
    // Returns multi-line string showing spectrum
    [[nodiscard]] std::string renderSpectrum() const;

    // Render simple ASCII constellation (8x8)
    [[nodiscard]] std::string renderConstellation() const;

    // Render all stats as formatted text
    [[nodiscard]] std::string renderStats() const;

    // Render complete display (combines all above)
    [[nodiscard]] std::string render() const;

    // Clear screen and render (for continuous display)
    void display() const;

    // Get current level in dBm (assumes 50 ohm)
    [[nodiscard]] double levelDbm() const;

    // Get S-meter reading
    [[nodiscard]] int sUnits() const;

    // Set reference level for dBm calculation
    void setRefLevel(double ref_dbm) { refLevel_dbm_ = ref_dbm; }

    // Reset visualizer state
    void reset();

private:
    void updateLevel(double mag);

    SpectrumAnalyzer spectrum_;

    // Level tracking
    double peakLevel_ = 0;
    double rmsLevel_ = 0;
    double sumMagSq_ = 0;
    size_t sampleCount_ = 0;

    // Constellation history (last N samples)
    static constexpr size_t CONSTELLATION_SIZE = 64;
    std::array<Complex, CONSTELLATION_SIZE> constellation_;
    size_t constellationPos_ = 0;

    // Reference level for dBm
    double refLevel_dbm_ = -73.0;  // S9 = -73 dBm

    // Timing
    std::chrono::steady_clock::time_point lastUpdate_;
    uint64_t totalSamples_ = 0;
};

//======================================================================
// Utility: ANSI color codes for terminal
//======================================================================
namespace term {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* CLEAR = "\033[2J\033[H";

    // Level-based color
    inline const char* levelColor(double db) {
        if (db > -50) return RED;      // Very strong
        if (db > -70) return YELLOW;   // Strong
        if (db > -90) return GREEN;    // Normal
        return CYAN;                    // Weak
    }
}

} // namespace nexrx
