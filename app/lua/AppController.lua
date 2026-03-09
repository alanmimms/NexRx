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
    QSD = false
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
    -- Watch VFO and update Preselector / Bands
    R.watch(function()
        local active = Model.rx.VFO.active:get()
        local freq = (active == "A") and Model.rx.VFO.A:get() or Model.rx.VFO.B:get()
        
        -- Update Bands system
        if bands and bands.setCurrent then
            bands.setCurrent(freq)
        end
        
        -- Auto-tune Preselector
        if Model.preselector.auto:get() then
            local L, C = PreselCal.calculate(freq)
            -- Use Model.set to override the projection (creates high-prio rule)
            Model.set("preselector.L", L)
            Model.set("preselector.capMask", C)
        end
        
        dirty.VFO = true
    end)

    -- Watch Preselector specific changes
    R.watch(function()
        Model.preselector.L:get()
        Model.preselector.capMask:get()
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
            local active = Model.rx.VFO.active:peek()
            Model.set("rx.VFO." .. active, freq)
        end
    end)
    
    -- Watch QSD changes
    R.watch(function()
        Model.rx.QSD.offsetK:get()
        dirty.QSD = true
    end)
end

-- =============================================================================
-- Hardware Synchronization
-- =============================================================================

--- Flush all dirty state to Hardware
-- Call this once per frame at the end of the update loop
function AppController.sync()
    local commands = {}
    local anyDirty = false

    if dirty.VFO then
        local active = Model.rx.VFO.active:peek()
        commands.VFO = (active == "A") and Model.rx.VFO.A:peek() or Model.rx.VFO.B:peek()
        dirty.VFO = false
        anyDirty = true
    end

    if dirty.preselector then
        commands.preselector = {
            L = Model.preselector.L:peek(),
            capMask = Model.preselector.capMask:peek()
        }
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
    
    if dirty.QSD then
        commands.QSD = {
            offsetK = Model.rx.QSD.offsetK:peek()
        }
        dirty.QSD = false
        anyDirty = true
    end

    if anyDirty then
        Hardware.sync(commands)
    end
end

return AppController
