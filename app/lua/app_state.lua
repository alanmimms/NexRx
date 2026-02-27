local R = require("reactive")
local animate = require("animate")
local setbox = require("setbox")
local bands = require("bands")

local AppState = {}

-- =============================================================================
-- Preselector Model (A priori calibration)
-- =============================================================================
local Preselector = {
    auto = true,
    -- Simple linear interpolation/approximation for now
    -- In a real system, this would be loaded via hw.getCalibration("preselector")
    calTable = {
        { f = 1.0e6,  L = true,  C = 0x7FF },
        { f = 3.5e6,  L = true,  C = 0x400 },
        { f = 7.0e6,  L = false, C = 0x600 },
        { f = 14.0e6, L = false, C = 0x300 },
        { f = 21.0e6, L = false, C = 0x150 },
        { f = 28.0e6, L = false, C = 0x080 },
    }
}

function Preselector.tune(freqHz)
    if not Preselector.auto then return end
    -- Find closest entry in calibration table
    local best = Preselector.calTable[1]
    for _, entry in ipairs(Preselector.calTable) do
        if math.abs(entry.f - freqHz) < math.abs(best.f - freqHz) then
            best = entry
        end
    end
    
    -- Update AppState properties (which triggers hardware sync)
    AppState.set("preselL1", best.L)
    for i = 0, 10 do
        local bit = math.floor(best.C / math.pow(2, i)) % 2
        AppState.set("preselC" .. i, bit == 1)
    end
end

-- =============================================================================
-- Property Specifications
-- =============================================================================

local specs = {
    frequency     = { min = 0.1, max = 30.0, setter = function(v) 
        local freqHz = v * 1e6
        print(string.format("[AppState] Tuning to %.6f MHz (%.0f Hz)", v, freqHz))
        -- Update VFO (this binding handles both local and remote)
        if rx and rx.setVfo then rx.setVfo(freqHz) end
        Preselector.tune(freqHz)
    end },
    vfoA          = { min = 0.1, max = 30.0 },
    vfoB          = { min = 0.1, max = 30.0 },
    activeVFO     = {},
    selectedMode  = { setter = function(v) 
        require("modes").setMode(v) 
        if v == "AM" then
            AppState.set("bandpassEnabled", true)
            AppState.set("bandpassWidth", 6000)
            AppState.set("bandpassCenter", 0)
        end
    end },
    selectedBand  = {},
    rxActive      = { defaultValue = false, setter = function(v)
        local dispatch = require("dispatch")
        if v then if audio and audio.start then audio.start() end; dispatch.setRxActive(true)
        else if audio and audio.stop then audio.stop() end; dispatch.setRxActive(false) end
    end },
    bandpassEnabled = { setter = function(v) rx.setBandpassEnabled(v) end },
    notchEnabled    = { setter = function(v) rx.setNotchEnabled(v) end },
    agcEnabled      = { setter = function(v) rx.setAgcEnabled(v) end },
    nrEnabled       = { setter = function(v) rx.setNrEnabled(v) end },
    nbEnabled       = { setter = function(v) rx.setNbEnabled(v) end },
    muteEnabled     = { setter = function(v) rx.setMute(v) end },
    testToneEnabled = { setter = function(v) audio.setTestTone(v, 440.0) end },
    bandpassCenter = { min = -5000, max = 5000, setter = function(v) rx.setBandpassCenter(v) end },
    bandpassWidth  = { min = 50, max = 4000, setter = function(v) rx.setBandpassWidth(v) end },
    notchCenter    = { min = -10000, max = 10000, setter = function(v) rx.setNotchCenter(v) end },
    notchWidth     = { min = 10, max = 500, setter = function(v) rx.setNotchWidth(v) end },
    volumeDb       = { min = -60, max = 0, setter = function(v) audio.setVolume(math.pow(10, v / 20)) end },
    squelch        = { min = 0, max = 1 },
    lmsMu          = { min = 0.00001, max = 0.1, setter = function(v) rx.setLmsMu(v) end },
    qsdOffsetK     = { min = 1, max = 24, setter = function(v) hw.setQsdOffset(v) end, requiresHw = true },
    rfAttenDb      = { min = 0, max = 45, step = 3, setter = function(v) hw.setAttenuation(v) end, requiresHw = true },
    preselL1       = { setter = function(v) hw.setPreselectorInd(v) end },
    preselC0       = { setter = function(v) hw.setPreselectorCap(0, v) end },
    preselC1       = { setter = function(v) hw.setPreselectorCap(1, v) end },
    preselC2       = { setter = function(v) hw.setPreselectorCap(2, v) end },
    preselC3       = { setter = function(v) hw.setPreselectorCap(3, v) end },
    preselC4       = { setter = function(v) hw.setPreselectorCap(4, v) end },
    preselC5       = { setter = function(v) hw.setPreselectorCap(5, v) end },
    preselC6       = { setter = function(v) hw.setPreselectorCap(6, v) end },
    preselC7       = { setter = function(v) hw.setPreselectorCap(7, v) end },
    preselC8       = { setter = function(v) hw.setPreselectorCap(8, v) end },
    preselC9       = { setter = function(v) hw.setPreselectorCap(9, v) end },
    preselC10      = { setter = function(v) hw.setPreselectorCap(10, v) end },
    wfMinDb        = { min = -140, max = -60 },
    wfMaxDb        = { min = -100, max = -20 },
    wfColormap     = {},
    wfBins         = { min = 128, max = 4096 },
    wfRows         = { min = 64, max = 1024 },
    isgEnabled     = { setter = function(v) 
        hw.setIsgEnable(v) 
    end },
    isgFrequency   = { min = 0.1, max = 30.0, setter = function(v) hw.setIsgFreq(v * 1e6) end },
    preselectorAuto = { defaultValue = true, setter = function(v) Preselector.auto = v end },
}

local observables = {}
local initialized = false
local activeAnimations = {}

function AppState.animateTo(name, targetValue, duration, easing)
    local obs = AppState.observable(name)
    if not obs then return end
    if not duration then
        local prevTags = setbox.getActiveTags()
        setbox.addTag("prop." .. name)
        duration = setbox.getNumber("anim_duration", 0.2)
        easing = easing or setbox.getString("anim_easing", "easeInOut")
        setbox.setActiveTags(prevTags)
    end
    if duration <= 0 then obs:set(targetValue); return end
    local wrapper = activeAnimations[name]
    if not wrapper then
        wrapper = { _name = name }
        setmetatable(wrapper, {
            __index = function(t, k) if k == "value" then return AppState.observable(t._name):get() end end,
            __newindex = function(t, k, v) if k == "value" then AppState.observable(t._name):set(v) end end
        })
        activeAnimations[name] = wrapper
    end
    animate.to(wrapper, "value", obs:get(), targetValue, duration, easing, function() activeAnimations[name] = nil end)
end

local function createObservable(name, spec, defaultValue)
    local obs = R.observable(defaultValue)
    local oldSet = obs.set
    obs.set = function(self, value)
        if spec.min ~= nil then value = math.max(spec.min, value) end
        if spec.max ~= nil then value = math.min(spec.max, value) end
        if spec.step then value = math.floor(value / spec.step + 0.5) * spec.step end
        local changed = oldSet(self, value)
        if changed and spec.setter then 
            R.untrack(function() 
                local ok, err = pcall(spec.setter, value) 
                if not ok then print("[AppState] Setter error for " .. name .. ": " .. tostring(err)) end
            end) 
        end
        return changed
    end
    return obs
end

function AppState.init()
    if initialized then return end
    print("[AppState] Initializing state...")
    local defaults = {
        frequency = setbox.getNumber("defaultFrequency", 14.200e6) / 1e6,
        vfoA = setbox.getNumber("vfoA", 14.200e6) / 1e6,
        vfoB = setbox.getNumber("vfoB", 7.050e6) / 1e6,
        activeVFO = setbox.getString("activeVFO", "A"),
        selectedMode = setbox.getString("defaultMode", "USB"),
        selectedBand = setbox.getString("defaultBand", "20m"),
        rxActive = setbox.getBool("rxActive", true),
        volumeDb = setbox.getNumber("volumeDb", -40),
        wfMinDb = setbox.getNumber("wfMinDb", -120),
        wfMaxDb = setbox.getNumber("wfMaxDb", -40),
        wfColormap = setbox.getString("wfColormap", "viridis"),
        wfBins = setbox.getNumber("wfBins", 512),
        wfRows = setbox.getNumber("wfRows", 256),
        preselectorAuto = true,
        isgEnabled = false,
        isgFrequency = 14.201,
    }
    AppState.batch(function()
        for name, spec in pairs(specs) do
            local defaultValue = defaults[name]
            if defaultValue == nil and spec.defaultValue ~= nil then defaultValue = spec.defaultValue end
            observables[name] = createObservable(name, spec, defaultValue)
            if defaultValue ~= nil and spec.setter then 
                R.untrack(function() 
                    local ok, err = pcall(spec.setter, defaultValue) 
                    if not ok then print("[AppState] Init setter error for " .. name .. ": " .. tostring(err)) end
                end) 
            end
        end
    end)
    AppState.watch("frequency", function(v)
        if observables.activeVFO:peek() == "A" then if observables.vfoA:peek() ~= v then observables.vfoA:set(v) end
        else if observables.vfoB:peek() ~= v then observables.vfoB:set(v) end end
        bands.setCurrent(v * 1e6)
    end)
    AppState.watch("activeVFO", function(v)
        local target = (v == "A") and observables.vfoA:peek() or observables.vfoB:peek()
        if observables.frequency:peek() ~= target then observables.frequency:set(target) end
    end)
    AppState.watch("vfoA", function(v) if observables.activeVFO:peek() == "A" and observables.frequency:peek() ~= v then observables.frequency:set(v) end end)
    AppState.watch("vfoB", function(v) if observables.activeVFO:peek() == "B" and observables.frequency:peek() ~= v then observables.frequency:set(v) end end)
    AppState.watch("selectedBand", function(b) local f = bands.getDefaultFreq(b); if f then AppState.set("frequency", f / 1e6) end end)
    AppState.watch("selectedMode", function(m) require("modes").setMode(m) end)
    initialized = true
end

function AppState.observable(name) return observables[name] end
function AppState.get(name)
    if observables[name] then return observables[name]:get() end
    return nil
end
function AppState.set(name, value) 
    if observables[name] then 
        local changed = observables[name]:set(value) 
        -- if changed then print("[AppState] Property " .. name .. " changed to " .. tostring(value)) end
        return changed
    end 
    return false 
end
function AppState.getSpec(name) return specs[name] end
function AppState.batch(fn) R.batch(fn) end
function AppState.watch(name, fn)
    if type(name) == "string" then
        local obs = AppState.observable(name)
        if obs then return R.watch(function() fn(obs:get()) end) end
    else return R.watch(name) end
end

return AppState
