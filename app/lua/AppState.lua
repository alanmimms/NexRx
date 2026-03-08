local R = require("Reactive")
local animate = require("Animate")
local setbox = require("SetBox")
local bands = require("Bands")

local AppState = {}

-- =============================================================================
-- Preselector Model (A priori calibration)
-- =============================================================================
local Preselector = {
    auto = true,
    internalUpdate = 0,
    -- Calibration table based on 200 Ohm LC Tank: 
    -- L1=220nH (always), L2=1.5uH (switchable) -> Total L=220nH (shorted) or 1.72uH (unshorted)
    -- L=true means L2 is shorted (220nH), L=false means L2 is in series (1.72uH)
    calTable = {
        { f = 1.0e6,  L = false, C = 0x740 }, -- 1.72uH, ~14.8nF
        { f = 3.5e6,  L = false, C = 0x0A0 }, -- 1.72uH, ~1.2nF
        { f = 7.0e6,  L = true,  C = 0x112 }, -- 220nH,  ~2.33nF
        { f = 14.0e6, L = true,  C = 0x040 }, -- 220nH,  ~560pF
        { f = 21.0e6, L = true,  C = 0x020 }, -- 220nH,  ~250pF
        { f = 28.0e6, L = true,  C = 0x011 }, -- 220nH,  ~128pF
    }
}

function Preselector.onManualCapChange()
    if Preselector.internalUpdate > 0 then return end
    AppState.set("preselectorAuto", false)
    if hw and hw.setPreselectorCap then hw.setPreselectorCap(Preselector.getCurrentCapMask()) end
end

function Preselector.getCurrentCapMask()
    local mask = 0
    for i = 0, 10 do
        if AppState.get("preselC" .. i) then
            mask = mask + (2 ^ i)
        end
    end
    return mask
end

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
    Preselector.internalUpdate = Preselector.internalUpdate + 1
    AppState.set("preselL1", best.L)
    for i = 0, 10 do
        local bit = math.floor(best.C / (2 ^ i)) % 2
        AppState.set("preselC" .. i, bit == 1)
    end
    Preselector.internalUpdate = Preselector.internalUpdate - 1

    -- Send atomic hardware commands
    if hw then
        if hw.setPreselectorInd then hw.setPreselectorInd(best.L and 1 or 0) end
        if hw.setPreselectorCap then hw.setPreselectorCap(best.C) end
    end
end

-- =============================================================================
-- Property Specifications
-- =============================================================================

local specs = {
    frequency     = { defaultValue = 14.200e6, min = 0.1e6, max = 30.0e6, setter = function(v) 
        -- Update VFO (this binding handles both local and remote)
        if rx and rx.setVfo then rx.setVfo(v) end
        Preselector.tune(v)
    end },
    vfoA          = { defaultValue = 14.200e6, min = 0.1e6, max = 30.0e6, setter = function(v)
        if AppState.get("activeVFO") == "A" then AppState.set("frequency", v) end
    end },
    vfoB          = { defaultValue = 7.050e6, min = 0.1e6, max = 30.0e6, setter = function(v)
        if AppState.get("activeVFO") == "B" then AppState.set("frequency", v) end
    end },
    activeVFO     = { defaultValue = "A" },
    selectedMode  = { defaultValue = "USB", setter = function(v) 
        local modeHelper = require("Modes")
        modeHelper.setMode(v) 
        if v == "AM" then
            AppState.set("bandpassEnabled", true)
            AppState.set("bandpassWidth", 6000)
            AppState.set("bandpassCenter", 0)
        end
    end },
    selectedBand  = { defaultValue = "20m" },
    rxActive      = { defaultValue = false, setter = function(v)
        local dispatch = require("Dispatch")
        if v then 
            if audio and audio.start then audio.start() end
            dispatch.setRxActive(true)
        else 
            if audio and audio.stop then audio.stop() end
            dispatch.setRxActive(false) 
        end
    end },
    bandpassEnabled = { defaultValue = true, setter = function(v) if rx and rx.setBandpassEnabled then rx.setBandpassEnabled(v) end end },
    notchEnabled    = { defaultValue = false, setter = function(v) if rx and rx.setNotchEnabled then rx.setNotchEnabled(v) end end },
    agcEnabled      = { defaultValue = false, setter = function(v) if rx and rx.setAgcEnabled then rx.setAgcEnabled(v) end end },
    nrEnabled       = { defaultValue = false, setter = function(v) if rx and rx.setNrEnabled then rx.setNrEnabled(v) end end },
    nbEnabled       = { defaultValue = false, setter = function(v) if rx and rx.setNbEnabled then rx.setNbEnabled(v) end end },
    volumeDb      = { defaultValue = -20, min = -60, max = 0, setter = function(v) if audio and audio.setVolume then audio.setVolume(v) end end },
    muteEnabled   = { defaultValue = false, setter = function(v) if rx and rx.setMute then rx.setMute(v) end end },
    testToneEnabled = { defaultValue = false, setter = function(v) 
        print("[AppState] testToneEnabled setter: " .. tostring(v))
        if audio and audio.setTestTone then audio.setTestTone(v, 440.0) end 
    end },
    demodFilterEnabled = { defaultValue = true, setter = function(v) if rx and rx.setDemodFilterEnabled then rx.setDemodFilterEnabled(v) end end },
    rfGainDb       = { defaultValue = 20, min = -20, max = 60, setter = function(v) if hw and hw.setRfGain then hw.setRfGain(v) end end },
    agcMode        = { defaultValue = 0, min = 0, max = 3, setter = function(v) if hw and hw.setAGCMode then hw.setAGCMode(v) end end },
    isgFrequency   = { defaultValue = 14.205e6, min = 0.1e6, max = 30.0e6, setter = function(v) if hw and hw.setIsgFreq then hw.setIsgFreq(v) end end },
    isgEnabled     = { defaultValue = false, setter = function(v) if hw and hw.setIsgEnable then hw.setIsgEnable(v) end end },
    qsdOffsetK     = { defaultValue = 12.0, min = -50.0, max = 50.0, setter = function(v) if hw and hw.setQsdOffset then hw.setQsdOffset(v) end end },
    
    notchCenter    = { defaultValue = 1000, min = 100, max = 5000, setter = function(v) if rx and rx.setNotchCenter then rx.setNotchCenter(v) end end },
    notchWidth     = { defaultValue = 100, min = 10, max = 1000, setter = function(v) if rx and rx.setNotchWidth then rx.setNotchWidth(v) end end },
    bandpassWidth  = { defaultValue = 2800, min = 100, max = 10000, setter = function(v) if rx and rx.setBandpassWidth then rx.setBandpassWidth(v) end end },
    bandpassCenter = { defaultValue = 1500, min = -5000, max = 5000, setter = function(v) if rx and rx.setBandpassCenter then rx.setBandpassCenter(v) end end },
    lmsMu          = { defaultValue = 0.01, min = 0.0001, max = 0.5, setter = function(v) if rx and rx.setLmsMu then rx.setLmsMu(v) end end },
    
    preselectorEnabled = { defaultValue = true, setter = function(v) if hw and hw.setPreselectorEnabled then hw.setPreselectorEnabled(v) end end },
    preselL1       = { defaultValue = false, setter = function(v) 
        if Preselector.internalUpdate == 0 then 
            AppState.set("preselectorAuto", false) 
            if hw and hw.setPreselectorInd then hw.setPreselectorInd(v and 1 or 0) end
        end
    end },
    preselC0       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC1       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC2       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC3       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC4       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC5       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC6       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC7       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC8       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC9       = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    preselC10      = { defaultValue = false, setter = function(v) if Preselector.internalUpdate == 0 then Preselector.onManualCapChange() end end },
    
    preselectorAuto = { defaultValue = true, setter = function(v) 
        Preselector.auto = v 
        if v then Preselector.tune(AppState.get("frequency")) end
    end },
    
    wfMinDb        = { defaultValue = -120, min = -140, max = -60 },
    wfMaxDb        = { defaultValue = -40, min = -80, max = 0 },
    wfColormap     = { defaultValue = "viridis" },
    wfBins         = { defaultValue = 512 },
    wfRows         = { defaultValue = 256 },
}

local observables = {}
local initialized = false

local function createObservable(name, spec, initialValue)
    local obs = R.observable(initialValue)
    local oldSet = obs.set
    obs.set = function(self, val)
        if spec.min and spec.max then val = math.max(spec.min, math.min(spec.max, val)) end
        local changed = oldSet(self, val)
        if changed and spec.setter and not R.isBatching() then
            spec.setter(val)
        end
        return changed
    end
    return obs
end

function AppState.init()
    if initialized then return end

    local hwState = nil
    if hw and hw.getState then hwState = hw.getState() end

    local autoInit = setbox.getBool("preselectorAuto", true)
    Preselector.auto = autoInit

    AppState.batch(function()
        for name, spec in pairs(specs) do
            local valueToSet = nil
            if setbox.has(name) then
                valueToSet = setbox.get(name)
            else
                valueToSet = spec.defaultValue
            end
            
            if hwState then
                if name == "frequency" and hwState.vfo then valueToSet = hwState.vfo
                elseif name == "vfoA" and hwState.vfoA then valueToSet = hwState.vfoA
                elseif name == "vfoB" and hwState.vfoB then valueToSet = hwState.vfoB
                elseif name == "rfAttenDb" and hwState.atten then valueToSet = hwState.atten
                elseif name == "agcMode" and hwState.agc ~= nil then valueToSet = hwState.agc
                elseif name == "isgFrequency" and hwState.isgFreq then valueToSet = hwState.isgFreq
                elseif name == "isgEnabled" and hwState.isgEn ~= nil then valueToSet = hwState.isgEn
                elseif name == "preselectorEnabled" and hwState.psEn ~= nil then valueToSet = hwState.psEn
                elseif name == "preselL1" and hwState.psL ~= nil then valueToSet = (hwState.psL == 1)
                elseif name:find("preselC") and hwState.psC ~= nil then
                    local idx = tonumber(name:match("preselC(%d+)"))
                    if idx then
                        local bit = math.floor(hwState.psC / (2 ^ idx)) % 2
                        valueToSet = (bit == 1)
                    end
                end
            end

            if name == "frequency" and valueToSet == spec.defaultValue then
                -- Try frequency from SetBox first, then defaultFrequency
                local freqVal = setbox.getNumber("frequency")
                if freqVal then 
                    valueToSet = freqVal
                    print("[AppState] Initialized frequency from SetBox 'frequency': " .. valueToSet .. " Hz")
                else
                    local defFreqHz = setbox.getNumber("defaultFrequency")
                    if defFreqHz then 
                        valueToSet = defFreqHz
                        print("[AppState] Initialized frequency from SetBox 'defaultFrequency': " .. valueToSet .. " Hz")
                    end
                end
            end
            if name == "preselectorAuto" then valueToSet = autoInit end
            if name == "frequency" and valueToSet then print("[AppState] Final frequency for " .. name .. ": " .. tostring(valueToSet)) end
            observables[name] = createObservable(name, spec, valueToSet)
        end

        Preselector.internalUpdate = Preselector.internalUpdate + 1
        for name, spec in pairs(specs) do
            if spec.setter then
                local val = observables[name]:peek()
                if val ~= nil then
                    R.untrack(function() 
                        local ok, err = pcall(spec.setter, val) 
                    end) 
                end
            end
        end
        Preselector.internalUpdate = Preselector.internalUpdate - 1
    end)

    Preselector.tune(AppState.get("frequency"))

    AppState.watch("frequency", function(v)
        if observables.activeVFO:peek() == "A" then 
            if observables.vfoA:peek() ~= v then observables.vfoA:set(v) end
        else 
            if observables.vfoB:peek() ~= v then observables.vfoB:set(v) end 
        end
        bands.setCurrent(v)
    end)

    AppState.watch("activeVFO", function(v)
        local target = (v == "A") and observables.vfoA:peek() or observables.vfoB:peek()
        if observables.frequency:peek() ~= target then observables.frequency:set(target) end
    end)

    AppState.watch("vfoA", function(v) 
        if observables.activeVFO:peek() == "A" and observables.frequency:peek() ~= v then observables.frequency:set(v) end 
    end)

    AppState.watch("vfoB", function(v) 
        if observables.activeVFO:peek() == "B" and observables.frequency:peek() ~= v then observables.frequency:set(v) end 
    end)

    AppState.watch("selectedBand", function(b) 
        local f = bands.getDefaultFreq(b)
        if f then AppState.set("frequency", f) end 
    end)

    AppState.watch("selectedMode", function(m) require("Modes").setMode(m) end)

    initialized = true
end

function AppState.observable(name) return observables[name] end
function AppState.get(name)
    if observables[name] then return observables[name]:get() end
    return nil
end
function AppState.set(name, value) 
    if observables[name] then return observables[name]:set(value) end 
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

function AppState.animateTo(name, targetValue)
    local obs = observables[name]
    if not obs then return end
    
    local prevTags = setbox.getActiveTags()
    setbox.addTag("prop." .. name)
    local duration = setbox.getNumber("anim_duration", 0.2)
    local easing = setbox.getString("anim_easing", "easeInOut")
    setbox.setActiveTags(prevTags)
    
    animate.property(obs, targetValue, duration, easing)
end

return AppState
