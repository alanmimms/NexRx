// NexRx Digital Twin - Stimulus Lua Bindings
//
// Provides Lua scripting interface for stimulus configuration.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "StimulusManager.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <memory>
#include <string>

namespace nexrx {

//======================================================================
// Stimulus Lua Bindings
//
// Registers the 'stimulus' table in Lua with functions to create
// and manage antenna stimuli:
//   stimulus.addMorse(name, {freq, amplitude, text, wpm, repeat})
//   stimulus.addSsb(name, {freq, amplitude, mode, tones/voice})
//   stimulus.addTone(name, {freq, amplitude})
//   stimulus.addNoise(name, {rms, type})
//   stimulus.remove(name)
//   stimulus.list()
//   stimulus.clear()
//======================================================================
class StimulusLua {
public:
    // Create bindings with a new StimulusManager
    StimulusLua();

    // Create bindings with existing StimulusManager
    explicit StimulusLua(std::shared_ptr<StimulusManager> manager);

    // Register bindings in Lua state
    void registerBindings(sol::state& lua);

    // Load and execute a stimulus config script
    bool loadScript(sol::state& lua, const std::string& path);

    // Get the StimulusManager
    std::shared_ptr<StimulusManager> manager() { return manager_; }

private:
    std::shared_ptr<StimulusManager> manager_;
};

} // namespace nexrx
