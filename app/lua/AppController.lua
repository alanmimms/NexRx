--[[
    AppController.lua - Coordination & Business Logic for NexRx
    
    The Controller manages state mutations, coordinates between Model
    properties, and synchronizes changes to Hardware.
]]

local R = require("Reactive")
local Model = require("Model")
local Hardware = require("Hardware")
local bands = require("Bands")
local smeter = require("ui.SMeter")

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

-- =============================================================================
-- Event Handlers (Registered with Events system)
-- =============================================================================

local events = require("Events")

function AppController.registerHandlers()
    -- VFO Control (Wheel/Arrows)
    events.registerHandler("vfo_control", function(event, widget, props)
        local prop = props.property or "rx.VFO.activeValue"
        local step = props.step or 100
        local current = 0
        
        if prop == "rx.VFO.activeValue" then
            local box = Model.getSelectedSignalBox()
            current = box and box.frequency or 14.2e6
        else
            current = setbox.getNumber(prop)
        end
        
        local delta = 0
        if event.type == events.Type.MOUSE_WHEEL then
            delta = event.delta * step
        elseif event.type == events.Type.KEY_DOWN then
            if event.key == "RIGHT" or event.key == "UP" then delta = step
            elseif event.key == "LEFT" or event.key == "DOWN" then delta = -step end
        end
        
        if delta ~= 0 then
            Model.set(prop, current + delta)
            return true
        end
        return false
    end)

    -- Frequency Entry
    events.registerHandler("freq_entry_start", function(event, widget)
        _G.freqEntryText = ""
        _G.freqEntryCursor = 0
        events.addModeTag("state.FreqEntryMode")
        return true
    end)

    events.registerHandler("freq_entry_text", function(event, widget)
        if not event.text then return false end
        _G.freqEntryText = _G.freqEntryText .. event.text
        _G.freqEntryCursor = #_G.freqEntryText
        return true
    end)

    events.registerHandler("freq_entry_confirm", function(event, widget)
        local val = tonumber(_G.freqEntryText)
        if val then
            -- Handle MHz vs Hz (simple heuristic: < 1000 = MHz)
            if val < 1000 then val = val * 1e6 end
            Model.set("rx.VFO.activeValue", val)
        end
        _G.freqEntryText = ""
        events.removeModeTag("state.FreqEntryMode")
        return true
    end)

    events.registerHandler("freq_entry_cancel", function(event, widget)
        _G.freqEntryText = ""
        events.removeModeTag("state.FreqEntryMode")
        return true
    end)

    -- SignalBox Management
    events.registerHandler("sb_select", function(event, widget)
        if not widget or not widget.data or not widget.data.index then return false end
        Model.selectedSignalBoxIndex:set(widget.data.index)
        return true
    end)

    events.registerHandler("sb_drag_start", function(event, widget)
        if not widget or not widget.data or not widget.data.index then return false end
        Model.selectedSignalBoxIndex:set(widget.data.index)
        AppController.dragState = {
            index = widget.data.index,
            startFreq = Model.getSelectedSignalBox().frequency,
            startX = event.x
        }
        return true
    end)

    events.registerHandler("sb_drag_move", function(event, widget)
        if not AppController.dragState then return false end
        local ds = AppController.dragState
        local box = Model.signalBoxes:peek()[ds.index]
        if not box then return false end

        -- Calculate frequency change based on pixels moved
        -- Need spectrum width and span
        local specW = widget and widget.bounds.w or 1000
        local zoom = Model.waterfall.zoom:get() or 1.0
        local span = _G.sampleRate / zoom
        local hzPerPx = span / specW
        
        local dx = event.x - ds.startX
        local newFreq = ds.startFreq + dx * hzPerPx
        
        -- Handle edge warping
        local margin = 10 -- pixels
        if event.x < widget.bounds.x + margin then
            -- Scroll spectrum left
            local shift = span * 0.1
            Model.spectrumCenterFreq:set(Model.spectrumCenterFreq:peek() - shift)
            ds.startX = ds.startX + (shift / hzPerPx)
        elseif event.x > widget.bounds.x + widget.bounds.w - margin then
            -- Scroll spectrum right
            local shift = span * 0.1
            Model.spectrumCenterFreq:set(Model.spectrumCenterFreq:peek() + shift)
            ds.startX = ds.startX - (shift / hzPerPx)
        end

        box.frequency = newFreq
        return true
    end)

    events.registerHandler("sb_drag_end", function(event, widget)
        AppController.dragState = nil
        return true
    end)

    -- Tab through SignalBoxes
    events.registerHandler("sb_next", function(event, widget)
        local boxes = Model.signalBoxes:peek()
        local idx = Model.selectedSignalBoxIndex:peek()
        idx = (idx % #boxes) + 1
        Model.selectedSignalBoxIndex:set(idx)
        return true
    end)

    -- Naming
    events.registerHandler("sb_name_start", function(event, widget)
        local box = Model.getSelectedSignalBox()
        if not box then return false end
        
        _G.sbNamingText = box.name or ""
        _G.sbNamingCursor = #_G.sbNamingText
        events.addModeTag("state.SbNamingMode")
        return true
    end)

    events.registerHandler("sb_name_text", function(event, widget)
        if not event.text then return false end
        _G.sbNamingText = _G.sbNamingText .. event.text
        _G.sbNamingCursor = #_G.sbNamingText
        return true
    end)

    events.registerHandler("sb_name_confirm", function(event, widget)
        local box = Model.getSelectedSignalBox()
        if box then
            box.name = _G.sbNamingText
        end
        _G.sbNamingText = ""
        events.removeModeTag("state.SbNamingMode")
        return true
    end)

    events.registerHandler("sb_name_cancel", function(event, widget)
        _G.sbNamingText = ""
        events.removeModeTag("state.SbNamingMode")
        return true
    end)
end

function AppController.init()
    AppController.registerHandlers()
    -- Watch selected signal box frequency and sync to hardware
    R.watch(function()
        local box = Model.getSelectedSignalBox()
        if not box then return end
        
        local freq = box.frequency
        local mode = box.mode
        
        -- Update Bands system
        if bands and bands.setCurrent then
            bands.setCurrent(freq)
            local currentBand = bands.getCurrent()
            if currentBand ~= "OOB" and Model.rx.selectedBand:peek() ~= currentBand then
                Model.rx.selectedBand:set(currentBand)
            end
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
        
        local box = Model.getSelectedSignalBox()
        if box then local _ = box.frequency end
        
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
        -- Temporarily disabled to ensure it doesn't overwrite manual changes
        --[[
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
        ]]
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
            
            commands.VFO = loFreq
            commands.tuningOffset = tuneHz
            print(string.format("[AppController] VFO Sync: LO=%.3f MHz, Tune=%.3f kHz", loFreq/1e6, tuneHz/1000.0))
        end
        dirty.VFO = false
        anyDirty = true
    end

    if dirty.preselector then
        local p = Model.preselector
        local autoEn = p.auto:peek()
        local box = Model.getSelectedSignalBox()
        local freq = box and box.frequency or 14.2e6
        
        commands.preselector = {
            enabled = p.enabled:peek(),
            autoTune = autoEn
        }
        
        if autoEn then
            local L, C = PreselCal.calculate(freq)
            commands.preselector.L = L
            commands.preselector.capMask = C
        else
            commands.preselector.L = p.L:peek()
            commands.preselector.capMask = p.capMask:peek()
        end
        dirty.preselector = false
        anyDirty = true
    end

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
