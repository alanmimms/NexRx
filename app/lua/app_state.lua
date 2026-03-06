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
    hw.setPreselectorCap(Preselector.getCurrentCapMask())
end

function Preselector.getCurrentCapMask()
    local mask = 0
    for i = 0, 10 do
        if AppState.get("preselC" .. i) then
            mask = mask + math.pow(2, i)
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
        local bit = math.floor(best.C / math.pow(2, i)) % 2
        AppState.set("preselC" .. i, bit == 1)
    end
    Preselector.internalUpdate = Preselector.internalUpdate - 1

    -- Send atomic hardware commands
    hw.setPreselectorInd(best.L and 1 or 0)
    hw.setPreselectorCap(best.C)
end

-- =============================================================================
-- Property Specifications
-- =============================================================================

local specs = {
    frequency     = { defaultValue = 14.200, min = 0.1, max = 30.0, setter = function(v) 
        local freqHz = v * 1e6
        print(string.format("[AppState] Tuning to %.6f MHz (%.0f Hz)", v, freqHz))
        -- Update VFO (this binding handles both local and remote)
        if rx and rx.setVfo then rx.setVfo(freqHz) end
        Preselector.tune(freqHz)
    end },
    vfoA          = { defaultValue = 14.200, min = 0.1, max = 30.0 },
    vfoB          = { defaultValue = 7.050, min = 0.1, max = 30.0 },
    activeVFO     = { defaultValue = "A" },
    selectedMode  = { defaultValue = "USB", setter = function(v) 
        local modeHelper = require("modes")
        modeHelper.setMode(v) 
        if v == "AM" then
            AppState.set("bandpassEnabled", true)
            AppState.set("bandpassWidth", 6000)
            AppState.set("bandpassCenter", 0)
        end
    end },
    selectedBand  = { defaultValue = "20m" },
    rxActive      = { defaultValue = false, setter = function(v)
        local dispatch = require("dispatch")
        if v then if audio and audio.start then audio.start() end; dispatch.setRxActive(true)
        else if audio and audio.stop then audio.stop() end; dispatch.setRxActive(false) end
    end },
    bandpassEnabled = { defaultValue = true, setter = function(v) rx.setBandpassEnabled(v) end },
    notchEnabled    = { defaultValue = false, setter = function(v) rx.setNotchEnabled(v) end },
    agcEnabled      = { defaultValue = true, setter = function(v) rx.setAgcEnabled(v) end },
    nrEnabled       = { defaultValue = false, setter = function(v) rx.setNrEnabled(v) end },
    nbEnabled       = { defaultValue = false, setter = function(v) rx.setNbEnabled(v) end },
    preselectorEnabled = { defaultValue = true, setter = function(v) hw.setPreselectorEnabled(v) end },
    muteEnabled     = { defaultValue = false, setter = function(v) rx.setMute(v) end },
    testToneEnabled = { defaultValue = false, setter = function(v) audio.setTestTone(v, 440.0) end },
    demodFilterEnabled = { defaultValue = true, setter = function(v) rx.setDemodFilterEnabled(v) end },
    bandpassCenter = { defaultValue = 700, min = -5000, max = 5000, setter = function(v) rx.setBandpassCenter(v) end },
    bandpassWidth  = { defaultValue = 2400, min = 50, max = 4000, setter = function(v) rx.setBandpassWidth(v) end },
    notchCenter    = { defaultValue = 0, min = -10000, max = 10000, setter = function(v) rx.setNotchCenter(v) end },
    notchWidth     = { defaultValue = 100, min = 10, max = 500, setter = function(v) rx.setNotchWidth(v) end },
    volumeDb       = { defaultValue = -6, min = -60, max = 20, setter = function(v) audio.setVolume(math.pow(10, v / 20)) end },
    squelch        = { defaultValue = 0.3, min = 0, max = 1 },
    lmsMu          = { defaultValue = 0.001, min = 0.00001, max = 0.1, setter = function(v) rx.setLmsMu(v) end },
    rfGainDb       = { defaultValue = 20, min = -20, max = 60, setter = function(v) hw.setRfGain(v) end },
    agcMode        = { defaultValue = 0, min = 0, max = 3, setter = function(v) hw.setAGCMode(v) end },
    qsdOffsetK     = { defaultValue = 12, min = 1, max = 24, setter = function(v) hw.setQsdOffset(v) end, requiresHw = true },
    rfAttenDb      = { defaultValue = 0, min = 0, max = 45, setter = function(v) hw.setAttenuation(v) end, requiresHw = true },
    preselL1       = { defaultValue = false, setter = function(v) 
        if Preselector.internalUpdate == 0 then 
            AppState.set("preselectorAuto", false) 
            hw.setPreselectorInd(v and 1 or 0)
        end
    end },
    preselC0       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC1       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC2       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC3       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC4       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC5       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC6       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC7       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC8       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC9       = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    preselC10      = { defaultValue = false, setter = function(v) if not Preselector.internalUpdate then Preselector.onManualCapChange() end end },
    wfMinDb        = { defaultValue = -120, min = -140, max = -60 },
    wfMaxDb        = { defaultValue = -40, min = -100, max = -20 },
    wfColormap     = { defaultValue = "viridis" },
    wfBins         = { defaultValue = 512, min = 128, max = 4096 },
    wfRows         = { defaultValue = 256, min = 64, max = 1024 },
    isgEnabled     = { defaultValue = false, setter = function(v) 
        hw.setIsgEnable(v) 
    end },
    isgFrequency   = { defaultValue = 14.201, min = 0.1, max = 30.0, setter = function(v) hw.setIsgFreq(v * 1e6) end },
    preselectorAuto = { defaultValue = true, setter = function(v) 
        Preselector.auto = v 
        if v then Preselector.tune(AppState.get("frequency") * 1e6) end
    end },
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

    local hwState = nil
    if hw and hw.getState then
        hwState = hw.getState()
        if hwState then print("[AppState] Received hardware state for initialization") end
    end

    -- Initial pass to ensure preselectorAuto exists before any setters run
    local autoInit = setbox.getBool("preselectorAuto", true)
    Preselector.auto = autoInit

    AppState.batch(function()
        -- Phase 1: Create all observables first
        for name, spec in pairs(specs) do
            local valueToSet = setbox.get(name)
            if valueToSet == nil then valueToSet = spec.defaultValue end
            
            -- Override with hardware state if available
            if hwState then
                if name == "frequency" and hwState.vfo then valueToSet = hwState.vfo / 1e6
                elseif name == "rfAttenDb" and hwState.atten then valueToSet = hwState.atten
                elseif name == "agcMode" and hwState.agc ~= nil then valueToSet = hwState.agc
                elseif name == "isgFrequency" and hwState.isgFreq then valueToSet = hwState.isgFreq / 1e6
                elseif name == "isgEnabled" and hwState.isgEn ~= nil then valueToSet = hwState.isgEn
                elseif name == "preselectorEnabled" and hwState.psEn ~= nil then valueToSet = hwState.psEn
                elseif name == "preselL1" and hwState.psL ~= nil then valueToSet = (hwState.psL == 1)
                elseif name:find("preselC") and hwState.psC ~= nil then
                    local idx = tonumber(name:match("preselC(%d+)"))
                    if idx then
                        local bit = math.floor(hwState.psC / math.pow(2, idx)) % 2
                        valueToSet = (bit == 1)
                    end
                end
            end

            if name == "frequency" and valueToSet == nil then
                local defFreq = setbox.getNumber("defaultFrequency")
                if defFreq then valueToSet = defFreq / 1e6 end
            end
            
            -- Use the early resolved autoInit for preselectorAuto
            if name == "preselectorAuto" then valueToSet = autoInit end
            
            observables[name] = createObservable(name, spec, valueToSet)
        end

        -- Phase 2: Call setters now that all targets exist
        Preselector.internalUpdate = Preselector.internalUpdate + 1
        for name, spec in pairs(specs) do
            if spec.setter then
                local val = observables[name]:peek()
                if val ~= nil then
                    R.untrack(function() 
                        local ok, err = pcall(spec.setter, val) 
                        if not ok then print("[AppState] Init setter error for " .. name .. ": " .. tostring(err)) end
                    end) 
                end
            end
        end
        Preselector.internalUpdate = Preselector.internalUpdate - 1
    end)

    -- Final synchronization: Force preselector to match initial frequency
    -- This ensures SPRC/SPRL commands are sent even if setter logic was deferred
    Preselector.tune(AppState.get("frequency") * 1e6)

    -- Set up cross-property watchers (for runtime updates)
    AppState.watch("frequency", function(v)
        if observables.activeVFO:peek() == "A" then 
            if observables.vfoA:peek() ~= v then observables.vfoA:set(v) end
        else 
            if observables.vfoB:peek() ~= v then observables.vfoB:set(v) end 
        end
        bands.setCurrent(v * 1e6)
    end)

    AppState.watch("activeVFO", function(v)
        local target = (v == "A") and observables.vfoA:peek() or observables.vfoB:peek()
        if observables.frequency:peek() ~= target then observables.frequency:set(target) end
    end)

    AppState.watch("vfoA", function(v) 
        if observables.activeVFO:peek() == "A" and observables.frequency:peek() ~= v then 
            observables.frequency:set(v) 
        end 
    end)

    AppState.watch("vfoB", function(v) 
        if observables.activeVFO:peek() == "B" and observables.frequency:peek() ~= v then 
            observables.frequency:set(v) 
        end 
    end)

    AppState.watch("selectedBand", function(b) 
        local f = bands.getDefaultFreq(b)
        if f then AppState.set("frequency", f / 1e6) end 
    end)

    AppState.watch("selectedMode", function(m) 
        require("modes").setMode(m) 
    end)

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
