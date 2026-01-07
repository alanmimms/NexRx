// NexRx Digital Twin - Morse Code Generator Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "MorseGenerator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace nexrx {

//======================================================================
// ITU Morse Code Table
// '.' = dit, '-' = dah
//======================================================================
namespace {

struct MorseEntry {
    char character;
    const char* pattern;
};

// ITU-R M.1677-1 standard Morse code
constexpr MorseEntry morseTable[] = {
    // Letters
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
    {'E', "."},     {'F', "..-."},  {'G', "--."},   {'H', "...."},
    {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
    {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},
    {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
    {'Y', "-.--"},  {'Z', "--.."},

    // Numbers
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},

    // Punctuation
    {'.', ".-.-.-"},  // Period
    {',', "--..--"},  // Comma
    {'?', "..--.."},  // Question mark
    {'\'', ".----."},// Apostrophe
    {'!', "-.-.--"}, // Exclamation (KW digraph)
    {'/', "-..-."},  // Slash
    {'(', "-.--."},  // Open parenthesis
    {')', "-.--.-"}, // Close parenthesis
    {'&', ".-..."},  // Ampersand (AS)
    {':', "---..."}, // Colon
    {';', "-.-.-."},// Semicolon
    {'=', "-...-"},  // Equals/Break
    {'+', ".-.-."},  // Plus (AR)
    {'-', "-....-"}, // Hyphen
    {'_', "..--.-"}, // Underscore
    {'"', ".-..-."}, // Quotation mark
    {'$', "...-..-"},// Dollar sign
    {'@', ".--.-."},// At sign

    // Prosigns (represented as single chars for convenience)
    {'<', "-...-"},  // BT (break/pause) - using < as placeholder
    {'>', "...-.-"}, // SK (end of contact) - using > as placeholder
};

constexpr size_t morseTableSize = sizeof(morseTable) / sizeof(morseTable[0]);

} // anonymous namespace

//======================================================================
// Implementation
//======================================================================

MorseGenerator::MorseGenerator(double freq_hz, double amplitude_v,
                               const std::string& text, double wpm)
    : freq_hz_(freq_hz)
    , amplitude_v_(amplitude_v)
    , text_(text)
    , wpm_(wpm)
{
    setWpm(wpm);  // Computes dit duration
    buildSequence();
}

void MorseGenerator::setText(const std::string& text) {
    text_ = text;
    buildSequence();
}

void MorseGenerator::setWpm(double wpm) {
    wpm_ = std::max(1.0, wpm);  // Minimum 1 WPM

    // PARIS standard: "PARIS" = 50 dit-lengths
    // At W WPM: 50 * W dits per minute
    // dit_duration = 60 / (50 * W) = 1.2 / W seconds
    ditDuration_s_ = 1.2 / wpm_;

    buildSequence();
}

const char* MorseGenerator::getMorsePattern(char c) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    for (size_t i = 0; i < morseTableSize; ++i) {
        if (morseTable[i].character == c) {
            return morseTable[i].pattern;
        }
    }
    return nullptr;
}

void MorseGenerator::buildSequence() {
    sequence_.clear();
    totalDuration_s_ = 0.0;

    double currentTime = 0.0;
    bool firstChar = true;

    for (size_t i = 0; i < text_.size(); ++i) {
        char c = text_[i];

        // Handle spaces (word gap)
        if (c == ' ') {
            // Word gap = 7 dits, but we already have 3 from char gap
            // So add 4 more dits worth
            if (!firstChar) {
                KeyEvent gap;
                gap.startTime = currentTime;
                gap.duration = 4.0 * ditDuration_s_;
                gap.keyDown = false;
                sequence_.push_back(gap);
                currentTime += gap.duration;
            }
            continue;
        }

        // Get Morse pattern
        const char* pattern = getMorsePattern(c);
        if (!pattern) {
            continue;  // Skip unknown characters
        }

        // Add inter-character gap (except for first character)
        if (!firstChar) {
            KeyEvent gap;
            gap.startTime = currentTime;
            gap.duration = 3.0 * ditDuration_s_;  // 3 dits between chars
            gap.keyDown = false;
            sequence_.push_back(gap);
            currentTime += gap.duration;
        }
        firstChar = false;

        // Process each element in the pattern
        bool firstElement = true;
        for (const char* p = pattern; *p; ++p) {
            // Add intra-character gap (1 dit between elements)
            if (!firstElement) {
                KeyEvent gap;
                gap.startTime = currentTime;
                gap.duration = ditDuration_s_;
                gap.keyDown = false;
                sequence_.push_back(gap);
                currentTime += gap.duration;
            }
            firstElement = false;

            // Add dit or dah
            KeyEvent element;
            element.startTime = currentTime;
            element.keyDown = true;

            if (*p == '.') {
                element.duration = ditDuration_s_;  // Dit = 1 unit
            } else if (*p == '-') {
                element.duration = 3.0 * ditDuration_s_;  // Dah = 3 units
            } else {
                continue;  // Unknown element
            }

            sequence_.push_back(element);
            currentTime += element.duration;
        }
    }

    totalDuration_s_ = currentTime;
}

double MorseGenerator::getEnvelope(double posInElement, double elementDuration) const {
    double envelopeTime_s = envelopeTime_ms_ / 1000.0;

    // Clamp envelope time to half the element duration
    envelopeTime_s = std::min(envelopeTime_s, elementDuration / 2.0);

    if (posInElement < envelopeTime_s) {
        // Rising edge - raised cosine
        double t = posInElement / envelopeTime_s;
        return 0.5 * (1.0 - std::cos(M_PI * t));
    } else if (posInElement > elementDuration - envelopeTime_s) {
        // Falling edge - raised cosine
        double t = (elementDuration - posInElement) / envelopeTime_s;
        return 0.5 * (1.0 - std::cos(M_PI * t));
    } else {
        // Full amplitude
        return 1.0;
    }
}

double MorseGenerator::getSample(double time_s) const {
    if (sequence_.empty()) {
        return 0.0;
    }

    // Handle repeat mode
    double effectiveTime = time_s;
    if (repeat_ && totalDuration_s_ > 0) {
        // Add a word gap at the end before repeating
        double cycleTime = totalDuration_s_ + 7.0 * ditDuration_s_;
        effectiveTime = std::fmod(time_s, cycleTime);
    }

    // Find which element we're in
    for (const auto& event : sequence_) {
        if (effectiveTime >= event.startTime &&
            effectiveTime < event.startTime + event.duration) {

            if (event.keyDown) {
                // Key is down - generate tone with envelope
                double posInElement = effectiveTime - event.startTime;
                double envelope = getEnvelope(posInElement, event.duration);
                double phase = 2.0 * M_PI * freq_hz_ * time_s;  // Use absolute time for phase continuity
                return amplitude_v_ * envelope * std::sin(phase);
            } else {
                // Key is up - silence
                return 0.0;
            }
        }
    }

    // Past end of sequence (in non-repeat mode or during end gap)
    return 0.0;
}

std::string MorseGenerator::description() const {
    std::ostringstream oss;
    oss << "Morse[" << freq_hz_ / 1e6 << "MHz, "
        << wpm_ << "WPM, \"" << text_.substr(0, 20);
    if (text_.size() > 20) oss << "...";
    oss << "\"]";
    return oss.str();
}

void MorseGenerator::reset() {
    // Nothing to reset - timing is calculated from absolute time
}

bool MorseGenerator::hasMore(double time_s) const {
    if (repeat_) {
        return true;  // Infinite when repeating
    }
    return time_s < totalDuration_s_;
}

double MorseGenerator::getEnvelope(double time_s) const {
    if (sequence_.empty()) {
        return 0.0;
    }

    // Handle repeat mode
    double effectiveTime = time_s;
    if (repeat_ && totalDuration_s_ > 0) {
        double cycleTime = totalDuration_s_ + 7.0 * ditDuration_s_;
        effectiveTime = std::fmod(time_s, cycleTime);
    }

    // Find which element we're in
    for (const auto& event : sequence_) {
        if (effectiveTime >= event.startTime &&
            effectiveTime < event.startTime + event.duration) {

            if (event.keyDown) {
                double posInElement = effectiveTime - event.startTime;
                return getEnvelope(posInElement, event.duration);
            } else {
                return 0.0;
            }
        }
    }

    return 0.0;
}

void MorseGenerator::getBasebandIQ(double time_s, double lo_freq_hz,
                                    double& out_i, double& out_q) const {
    double env = getEnvelope(time_s) * amplitude_v_;
    if (env < 1e-12) {
        out_i = out_q = 0.0;
        return;
    }

    // Baseband frequency = carrier - LO
    double baseband_freq = freq_hz_ - lo_freq_hz;

    // Generate baseband I/Q analytically (no RF aliasing!)
    double phase = 2.0 * M_PI * baseband_freq * time_s;
    out_i = env * std::cos(phase);
    out_q = env * std::sin(phase);
}

} // namespace nexrx
