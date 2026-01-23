--[[
    app_state.lua - Reactive Application State

    Replaces the state table in main.lua with reactive observables.
    Provides:
    - Observable properties for all application state
    - Computed properties for derived values
    - Watchers for C++ synchronization
    - Property specs for bounds/steps

    Usage:
        local AppState = require("app_state")

        -- Access reactive state
        AppState.frequency:set(14.200)
        local freq = AppState.frequency:get()

        -- Watch for changes
        AppState.watch("frequency", function(value)
            rx.setVfo(value * 1e6)
        end)
]]

local R = require("reactive")

local AppState = {}

-- =============================================================================
-- Property Specifications (bounds, steps, setters)
-- =============================================================================

local specs = {
    -- Radio
    frequency     = { min = 0.1, max = 30.0 },
    vfoA          = { min = 0.1, max = 30.0 },
    vfoB          = { min = 0.1, max = 30.0 },
    activeVFO     = {},  -- "A" or "B"
    selectedMode  = {},  -- "USB", "LSB", "CW", "AM"
    selectedBand  = {},  -- "20m", "40m", etc.

    -- DSP toggles
    bandpassEnabled = { setter = function(v) rx.setBandpassEnabled(v) end },
    notchEnabled    = { setter = function(v) rx.setNotchEnabled(v) end },
    agcEnabled      = { setter = function(v) rx.setAgcEnabled(v) end },
    nrEnabled       = { setter = function(v) rx.setNrEnabled(v) end },
    nbEnabled       = { setter = function(v) rx.setNbEnabled(v) end },
    muteEnabled     = { setter = function(v) rx.setMute(v) end },
    testToneEnabled = { setter = function(v) audio.setTestTone(v, 440.0) end },

    -- DSP parameters
    bandpassCenter = { min = -5000, max = 5000, setter = function(v) rx.setBandpassCenter(v) end },
    bandpassWidth  = { min = 50, max = 4000, setter = function(v) rx.setBandpassWidth(v) end },
    notchCenter    = { min = -10000, max = 10000, setter = function(v) rx.setNotchCenter(v) end },
    notchWidth     = { min = 10, max = 500, setter = function(v) rx.setNotchWidth(v) end },
    volumeDb       = { min = -60, max = 0, setter = function(v) audio.setVolume(math.pow(10, v / 20)) end },
    squelch        = { min = 0, max = 1 },
    lmsMu          = { min = 0.00001, max = 0.1, setter = function(v) rx.setLmsMu(v) end },

    -- Hardware
    qsdOffsetK     = { min = 1, max = 24, setter = function(v) hw.setQsdOffset(v) end, requiresHw = true },
    rfAttenDb      = { min = 0, max = 45, step = 3, setter = function(v) hw.setAttenuation(v) end, requiresHw = true },

    -- Display
    wfMinDb        = { min = -140, max = -60 },
    wfMaxDb        = { min = -100, max = -20 },
    wfColormap     = {},
    -- Layout constraints are NOT here - widgets query SetBox directly via their tags
}

-- =============================================================================
-- Create Observables
-- =============================================================================

local observables = {}
local watchers = {}

-- Create observable for each property with optional bounds enforcement
local function createObservable(name, spec, defaultValue)
    local obs = R.observable(defaultValue)

    -- Wrap set to enforce bounds and call setter
    local originalSet = obs.set
    obs.set = function(self, value)
        -- Clamp to bounds
        if spec.min ~= nil then value = math.max(spec.min, value) end
        if spec.max ~= nil then value = math.min(spec.max, value) end
        -- Quantize to step
        if spec.step then value = math.floor(value / spec.step + 0.5) * spec.step end

        originalSet(self, value)

        -- Call C++ setter if specified
        if spec.setter then
            if not spec.requiresHw or (hw and hw.isConnected and hw.isConnected()) then
                local ok, err = pcall(spec.setter, value)
                if not ok then
                    print("[AppState] Setter error for " .. name .. ": " .. tostring(err))
                end
            end
        end
    end

    return obs
end

-- =============================================================================
-- Initialize with defaults from SetBox
-- =============================================================================

function AppState.init()
    -- Load defaults from SetBox
    local defaults = {
        -- Radio
        frequency = setbox.getNumber("defaultFrequency", 14.200e6) / 1e6,
        vfoA = setbox.getNumber("vfoA", 14.200e6) / 1e6,
        vfoB = setbox.getNumber("vfoB", 7.050e6) / 1e6,
        activeVFO = setbox.getString("activeVFO", "A"),
        selectedMode = setbox.getString("defaultMode", "USB"),
        selectedBand = setbox.getString("defaultBand", "20m"),

        -- DSP toggles
        bandpassEnabled = setbox.getBool("bandpassEnabled", false),
        notchEnabled = setbox.getBool("notchEnabled", false),
        agcEnabled = setbox.getBool("agcEnabled", true),
        nrEnabled = setbox.getBool("nrEnabled", false),
        nbEnabled = setbox.getBool("nbEnabled", false),
        muteEnabled = setbox.getBool("muted", false),
        testToneEnabled = false,

        -- DSP parameters
        bandpassCenter = setbox.getNumber("bandpassCenter", 700),
        bandpassWidth = setbox.getNumber("bandpassWidth", 500),
        notchCenter = setbox.getNumber("notchCenter", 0),
        notchWidth = setbox.getNumber("notchWidth", 100),
        volumeDb = setbox.getNumber("volumeDb", -20),
        squelch = setbox.getNumber("squelch", 0.3),
        lmsMu = setbox.getNumber("lmsMu", 0.001),

        -- Hardware
        qsdOffsetK = setbox.getNumber("qsdOffsetK", 12.0),
        rfAttenDb = math.floor(setbox.getNumber("rfAttenDb", 0)),

        -- Display
        wfMinDb = setbox.getNumber("wfMinDb", -120),
        wfMaxDb = setbox.getNumber("wfMaxDb", -40),
        wfColormap = setbox.getString("wfColormap", "viridis"),
    }

    -- Create observables for all properties
    for name, spec in pairs(specs) do
        observables[name] = createObservable(name, spec, defaults[name])
    end

    -- Create computed properties
    AppState._createComputeds()

    print("[AppState] Initialized with reactive properties")
end

-- =============================================================================
-- Computed Properties
-- =============================================================================

local computeds = {}

function AppState._createComputeds()
    -- VFO display string
    computeds.frequencyDisplay = R.computed(function()
        return string.format("%.6f", observables.frequency:get())
    end)

    -- Active VFO frequency (follows activeVFO selection)
    computeds.activeVfoFrequency = R.computed(function()
        if observables.activeVFO:get() == "A" then
            return observables.vfoA:get()
        else
            return observables.vfoB:get()
        end
    end)
end

-- =============================================================================
-- Public API
-- =============================================================================

-- Get an observable by name
function AppState.observable(name)
    return observables[name]
end

-- Get a computed by name
function AppState.computed(name)
    return computeds[name]
end

-- Get property value (works for both observable and computed)
function AppState.get(name)
    if observables[name] then
        return observables[name]:get()
    elseif computeds[name] then
        return computeds[name]:get()
    end
    return nil
end

-- Set property value (observables only)
function AppState.set(name, value)
    if observables[name] then
        observables[name]:set(value)
    else
        error("Cannot set property: " .. name .. " (not an observable)")
    end
end

-- Watch a property for changes
function AppState.watch(name, fn)
    local target = observables[name] or computeds[name]
    if not target then
        error("Unknown property: " .. name)
    end

    local watcher = R.watch(function()
        local value = target:get()
        fn(value, name)
    end)

    table.insert(watchers, watcher)
    return watcher
end

-- Batch multiple changes
AppState.batch = R.batch

-- Get property spec
function AppState.getSpec(name)
    return specs[name]
end

-- Get all property names
function AppState.getPropertyNames()
    local names = {}
    for name in pairs(observables) do
        table.insert(names, name)
    end
    return names
end

-- =============================================================================
-- Metatable for convenient access: AppState.frequency instead of AppState.get("frequency")
-- =============================================================================

setmetatable(AppState, {
    __index = function(_, key)
        if observables[key] then
            return observables[key]
        elseif computeds[key] then
            return computeds[key]
        end
        return nil
    end,
})

-- =============================================================================
-- Serialization (for Design Mode round-trip)
-- =============================================================================

function AppState.toTable()
    local result = {}
    for name, obs in pairs(observables) do
        result[name] = obs:peek()
    end
    return result
end

function AppState.fromTable(data)
    R.batch(function()
        for name, value in pairs(data) do
            if observables[name] then
                observables[name]:set(value)
            end
        end
    end)
end

return AppState
