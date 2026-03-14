--[[
    AppController.lua - Coordination & Business Logic for NexRx
    
    The Controller manages state mutations, coordinates between Model
    properties, and synchronizes changes to Hardware.
]]

local R = require("Reactive")
local Model = require("Model")
local Hardware = require("Hardware")
local bands = require("Bands")

local AppController = {}

-- =============================================================================
-- Internal State
-- =============================================================================

local dirty = {
    VFO = false,
    preselector = false,
    AGC = false,
    RF = false,
    QSD = false,
    volume = false
}

-- =============================================================================
-- Preselector Calibration Logic
-- =============================================================================

local PreselCal = {
    -- L=true means L2 is shorted (220nH), L=false means L2 is in series (1.72uH)
    table = {
        { f = 1.0e6,  L = false, C = 0x740 }, -- 1.72uH, ~14.8nF
        { f = 3.5e6,  L = false, C = 0x0A0 }, -- 1.72uH, ~1.2nF
        { f = 7.0e6,  L = true,  C = 0x112 }, -- 220nH,  ~2.33nF
        { f = 14.0e6, L = true,  C = 0x040 }, -- 220nH,  ~560pF
        { f = 21.0e6, L = true,  C = 0x020 }, -- 220nH,  ~250pF
        { f = 28.0e6, L = true,  C = 0x011 }, -- 220nH,  ~128pF
    }
}

function PreselCal.calculate(freqHz)
    local best = PreselCal.table[1]
    for _, entry in ipairs(PreselCal.table) do
        if math.abs(entry.f - freqHz) < math.abs(best.f - freqHz) then
            best = entry
        end
    end
    return best.L, best.C
end

-- =============================================================================
-- Reactive Logic (Property Coordination)
-- =============================================================================

function AppController.init()
    -- Watch selected signal box frequency and sync to hardware
    R.watch(function()
        local box = Model.getSelectedSignalBox()
        if not box then return end
        
        local freq = box.frequency
        local mode = box.mode
        
        -- Update Bands system
        if bands and bands.setCurrent then
            bands.setCurrent(freq)
        end
        
        -- Automatic centering: if the signal moves off-screen, re-center
        local zoom = Model.waterfall.zoom:peek() or 1.0
        local span = _G.sampleRate / zoom
        local center = Model.spectrumCenterFreq:peek()
        local margin = span * 0.1 -- 10% margin
        
        if math.abs(freq - center) > (span / 2 - margin) then
            Model.spectrumCenterFreq:set(freq)
        end
        
        dirty.VFO = true
    end)

    -- Watch Preselector specific changes
    R.watch(function()
        Model.preselector.L:get()
        Model.preselector.capMask:get()
        Model.preselector.auto:get()
        Model.preselector.enabled:get()
        dirty.preselector = true
    end)

    -- Watch AGC changes
    R.watch(function()
        Model.rx.AGC.enabled:get()
        Model.rx.AGC.mode:get()
        dirty.AGC = true
    end)

    -- Watch RF changes
    R.watch(function()
        Model.rx.RF.gainDB:get()
        Model.rx.RF.attenuationDB:get()
        dirty.RF = true
    end)
    
    -- Watch selected mode
    R.watch(function()
        local m = Model.rx.selectedMode:get()
        local Modes = require("Modes")
        Modes.setMode(m)
    end)

    -- Watch selected band
    R.watch(function()
        local b = Model.rx.selectedBand:get()
        local freq = bands.getDefaultFreq(b)
        if freq then
            Model.set("rx.VFO.activeValue", freq)
        end
    end)

    -- Watch spectrum center changes
    R.watch(function()
        Model.spectrumCenterFreq:get()
        dirty.VFO = true
    end)
    
    -- Watch QSD changes
    R.watch(function()
        Model.rx.QSD.offsetK:get()
        dirty.QSD = true
    end)

    -- Watch volume changes
    R.watch(function()
        Model.rx.volume.DB:get()
        Model.rx.volume.muted:get()
        dirty.volume = true
    end)

    -- Watch CW Pitch
    R.watch(function()
        local pitch = Model.rx.CW.pitch:get()
        if rx and rx.setBfoOffset and pitch ~= nil then
            rx.setBfoOffset(pitch)
        end
    end)

    -- Watch test tone
    R.watch(function()
        local enabled = Model.rx.testToneEnabled:get()
        if audio and audio.setTestTone and enabled ~= nil then
            audio.setTestTone(enabled, 440.0)
        end
    end)
end

-- =============================================================================
-- Hardware Synchronization
-- =============================================================================

--- Flush all dirty state to Hardware
-- Call this once per frame at the end of the update loop
function AppController.pollState()
    if not Hardware.isHardwareEnabled() then return end
    
    local state = Hardware.getState()
    if not state then return end

    -- Update Preselector L/C from hardware IF auto-tune is ON
    if Model.preselector.auto:peek() then
        if state.psL ~= nil then
            local hwL = (state.psL == 1)
            if hwL ~= Model.preselector.L:peek() then
                Model.set("preselector.L", hwL)
            end
        end
        if state.psC ~= nil then
            if state.psC ~= Model.preselector.capMask:peek() then
                Model.set("preselector.capMask", state.psC)
            end
        end
    end
end

function AppController.sync()
    local commands = {}
    local anyDirty = false

    if dirty.VFO then
        local box = Model.getSelectedSignalBox()
        if box then
            local loFreq = Model.spectrumCenterFreq:peek()
            local sigFreq = box.frequency
            local tuneHz = sigFreq - loFreq
            
            -- Sync hardware VFO (LO) and DSP digital tuning offset
            Hardware.sync({
                VFO = loFreq,
                tuningOffset = tuneHz
            })
            print(string.format("[AppController] VFO Sync: LO=%.3f MHz, Tune=%.3f kHz", loFreq/1e6, tuneHz/1000.0))
        end
        dirty.VFO = false
        anyDirty = true
    end

    if dirty.preselector then
        local p = Model.preselector
        local autoEn = p.auto:get()
        commands.preselector = {
            enabled = p.enabled:get(),
            autoTune = autoEn
        }
        if not autoEn then
            commands.preselector.L = p.L:get()
            commands.preselector.capMask = p.capMask:get()
        end
        dirty.preselector = false
        anyDirty = true
    end

    if dirty.AGC then
        commands.AGC = {
            enabled = Model.rx.AGC.enabled:peek(),
            mode = Model.rx.AGC.mode:peek()
        }
        dirty.AGC = false
        anyDirty = true
    end

    if dirty.RF then
        commands.RF = {
            gainDB = Model.rx.RF.gainDB:peek(),
            attenuationDB = Model.rx.RF.attenuationDB:peek()
        }
        dirty.RF = false
        anyDirty = true
    end
    
    dirty.QSD = false -- Managed by VFO block now

    if dirty.volume then
        commands.volume = {
            DB = Model.rx.volume.DB:peek(),
            muted = Model.rx.volume.muted:peek()
        }
        dirty.volume = false
        anyDirty = true
    end

    if anyDirty then
        Hardware.sync(commands)
    end
end

return AppController
