// NexRx Digital Twin - Stimulus Lua Bindings Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "StimulusLua.hpp"
#include "MorseGenerator.hpp"
#include "SsbGenerator.hpp"
#include "SweepGenerator.hpp"
#include "TtsEngine.hpp"
#include "ToneGenerator.hpp"
#include "NoiseGenerator.hpp"

#include <iostream>
#include <fstream>

namespace nexrx {

StimulusLua::StimulusLua()
    : manager_(std::make_shared<StimulusManager>())
{}

StimulusLua::StimulusLua(std::shared_ptr<StimulusManager> manager)
    : manager_(std::move(manager))
{}

void StimulusLua::registerBindings(sol::state& lua) {
    // Create stimulus table
    sol::table stimulus = lua.create_named_table("stimulus");

    // stimulus.addMorse(name, config)
    stimulus["addMorse"] = [this](const std::string& name, sol::table config) {
        double freq = config.get_or("freq", 14.0e6);
        double amplitude = config.get_or("amplitude", 50e-6);  // ~S9 default
        std::string text = config.get_or<std::string>("text", "CQ");
        double wpm = config.get_or("wpm", 20.0);
        bool repeat = config.get_or("repeat", true);

        auto morse = std::make_shared<MorseGenerator>(freq, amplitude, text, wpm);
        morse->setRepeat(repeat);

        manager_->addStimulus(name, morse, "morse", freq, amplitude);

        std::cout << "[Stimulus] Added Morse '" << name << "': "
                  << freq / 1e6 << " MHz, " << wpm << " WPM, \""
                  << text.substr(0, 20) << (text.size() > 20 ? "..." : "")
                  << "\"" << std::endl;
    };

    // stimulus.addSsb(name, config)
    stimulus["addSsb"] = [this](const std::string& name, sol::table config) {
        double freq = config.get_or("freq", 14.2e6);
        double amplitude = config.get_or("amplitude", 50e-6);
        std::string modeStr = config.get_or<std::string>("mode", "usb");

        SsbGenerator::Mode mode = (modeStr == "lsb" || modeStr == "LSB")
            ? SsbGenerator::Mode::LSB
            : SsbGenerator::Mode::USB;

        auto ssb = std::make_shared<SsbGenerator>(freq, amplitude, mode);

        // Check for tones
        sol::optional<sol::table> tonesOpt = config["tones"];
        if (tonesOpt) {
            std::vector<double> tones;
            for (auto& kv : *tonesOpt) {
                tones.push_back(kv.second.as<double>());
            }
            ssb->setTones(tones);

            manager_->addStimulus(name, ssb, "ssb", freq, amplitude);

            std::cout << "[Stimulus] Added SSB '" << name << "': "
                      << freq / 1e6 << " MHz " << modeStr << ", "
                      << tones.size() << " tone(s)" << std::endl;
            return;
        }

        // Check for voice
        sol::optional<std::string> voiceOpt = config["voice"];
        if (voiceOpt) {
            auto tts = std::make_shared<TtsEngine>();
            tts->setText(*voiceOpt);
            tts->setRepeat(config.get_or("repeat", true));
            ssb->setVoice(tts, config.get_or("repeat", true));

            manager_->addStimulus(name, ssb, "ssb", freq, amplitude);

            std::cout << "[Stimulus] Added SSB voice '" << name << "': "
                      << freq / 1e6 << " MHz " << modeStr << std::endl;
            return;
        }

        // Default: 1kHz tone
        ssb->setTones({1000.0});
        manager_->addStimulus(name, ssb, "ssb", freq, amplitude);

        std::cout << "[Stimulus] Added SSB '" << name << "': "
                  << freq / 1e6 << " MHz " << modeStr << ", default tone" << std::endl;
    };

    // stimulus.addTone(name, config)
    stimulus["addTone"] = [this](const std::string& name, sol::table config) {
        double freq = config.get_or("freq", 14.0e6);
        double amplitude = config.get_or("amplitude", 50e-6);

        auto tone = std::make_shared<ToneGenerator>(freq, amplitude);

        // Check for additional tones
        sol::optional<sol::table> tonesOpt = config["tones"];
        if (tonesOpt) {
            for (auto& kv : *tonesOpt) {
                sol::table t = kv.second.as<sol::table>();
                double f = t.get_or("freq", freq);
                double a = t.get_or("amplitude", amplitude);
                double p = t.get_or("phase", 0.0);
                tone->addTone(f, a, p);
            }
        }

        manager_->addStimulus(name, tone, "tone", freq, amplitude);

        std::cout << "[Stimulus] Added Tone '" << name << "': "
                  << freq / 1e6 << " MHz" << std::endl;
    };

    // stimulus.addSweep(name, config)
    // Frequency sweep for passband measurement
    stimulus["addSweep"] = [this](const std::string& name, sol::table config) {
        double startFreq = config.get_or("start", 1.0e6);   // Default 1 MHz
        double endFreq = config.get_or("end", 30.0e6);      // Default 30 MHz
        double amplitude = config.get_or("amplitude", 50e-6); // ~S9 default
        double rate = config.get_or("rate", 1.0e6);         // Default 1 MHz/s

        // Alternative: specify duration instead of rate
        sol::optional<double> durationOpt = config["duration"];
        if (durationOpt) {
            rate = std::abs(endFreq - startFreq) / *durationOpt;
        }

        auto sweep = std::make_shared<SweepGenerator>(startFreq, endFreq, amplitude, rate);

        // Sweep mode
        std::string modeStr = config.get_or<std::string>("mode", "linear");
        if (modeStr == "log" || modeStr == "logarithmic") {
            sweep->setSweepMode(SweepGenerator::SweepMode::Logarithmic);
        }

        // Repeat mode
        std::string repeatStr = config.get_or<std::string>("repeat", "repeat");
        if (repeatStr == "once" || repeatStr == "oneshot") {
            sweep->setRepeatMode(SweepGenerator::RepeatMode::OneShot);
        } else if (repeatStr == "pingpong" || repeatStr == "bounce") {
            sweep->setRepeatMode(SweepGenerator::RepeatMode::PingPong);
        } else {
            sweep->setRepeatMode(SweepGenerator::RepeatMode::Repeat);
        }

        manager_->addStimulus(name, sweep, "sweep", startFreq, amplitude);

        double duration = sweep->sweepDuration();
        std::cout << "[Stimulus] Added Sweep '" << name << "': "
                  << startFreq / 1e6 << "-" << endFreq / 1e6 << " MHz, "
                  << rate / 1e6 << " MHz/s (" << duration << "s sweep)"
                  << std::endl;
    };

    // stimulus.addNoise(name, config)
    stimulus["addNoise"] = [this](const std::string& name, sol::table config) {
        double rms = config.get_or("rms", 10e-6);  // 10µV default
        std::string type = config.get_or<std::string>("type", "thermal");

        std::shared_ptr<NoiseGenerator> noise;
        if (type == "thermal") {
            // Thermal noise at room temperature, 3kHz bandwidth
            noise = std::make_shared<NoiseGenerator>(
                NoiseGenerator::thermal(290.0, 3000.0));
            // Scale to requested RMS
            double thermalRms = noise->getSample(0);  // Get baseline
            noise = std::make_shared<NoiseGenerator>(rms);
        } else {
            noise = std::make_shared<NoiseGenerator>(rms);
        }

        manager_->addStimulus(name, noise, "noise", 0.0, rms);

        std::cout << "[Stimulus] Added Noise '" << name << "': "
                  << rms * 1e6 << " µV RMS" << std::endl;
    };

    // stimulus.remove(name)
    stimulus["remove"] = [this](const std::string& name) {
        bool removed = manager_->removeStimulus(name);
        if (removed) {
            std::cout << "[Stimulus] Removed '" << name << "'" << std::endl;
        }
        return removed;
    };

    // stimulus.clear()
    stimulus["clear"] = [this]() {
        manager_->clearAll();
        std::cout << "[Stimulus] Cleared all stimuli" << std::endl;
    };

    // stimulus.enable(name, enabled)
    stimulus["enable"] = [this](const std::string& name, bool enabled) {
        manager_->setEnabled(name, enabled);
    };

    // stimulus.list() -> table of {name -> {type, freq, amplitude, active}}
    stimulus["list"] = [this](sol::this_state s) {
        sol::state_view lua(s);
        sol::table result = lua.create_table();

        for (const auto& info : manager_->listInfo()) {
            sol::table entry = lua.create_table();
            entry["type"] = info.type;
            entry["freq"] = info.frequency_hz;
            entry["amplitude"] = info.amplitude_v;
            entry["active"] = info.active;
            result[info.name] = entry;
        }

        return result;
    };

    // stimulus.count() -> number
    stimulus["count"] = [this]() {
        return manager_->count();
    };

    std::cout << "[Stimulus] Lua bindings registered" << std::endl;
}

bool StimulusLua::loadScript(sol::state& lua, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Stimulus] Failed to open script: " << path << std::endl;
        return false;
    }

    try {
        sol::protected_function_result result = lua.safe_script_file(path);
        if (!result.valid()) {
            sol::error err = result;
            std::cerr << "[Stimulus] Script error: " << err.what() << std::endl;
            return false;
        }
        std::cout << "[Stimulus] Loaded script: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Stimulus] Script exception: " << e.what() << std::endl;
        return false;
    }
}

} // namespace nexrx
