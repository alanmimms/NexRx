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
-- BPF Selection Logic
-- =============================================================================

local BpfCal = {
    ranges = {
        { start = 1.8e6,  stop = 3.4e6,  index = 1 },
        { start = 3.2e6,  stop = 7.5e6,  index = 2 },
        { start = 7.3e6,  stop = 14.5e6, index = 3 },
        { start = 14.3e6, stop = 22.0e6, index = 4 },
        { start = 21.8e6, stop = 30.0e6, index = 5 }
    }
}

function BpfCal.calculate(freqHz)
    -- Simple selection with preference for lower index on overlaps
    for _, r in ipairs(BpfCal.ranges) do
        if freqHz >= r.start and freqHz <= r.stop then
            return r.index
        end
    end
    return 0 -- Bypass/None
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
        if event.type == "mouseWheel" then
            delta = event.delta * step
        elseif event.type == "key" then
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
        local margin = span * 0.02 -- 2% margin (less aggressive)
        
        -- Don't auto-center if the user is currently dragging a signal box
        local activeId = require("ui.State").getActive()
        local isDragging = activeId and activeId:match("^sb%-")
        
        if not isDragging and math.abs(freq - center) > (span / 2 - margin) then
            Model.spectrumCenterFreq:set(freq)
        end
        
        dirty.VFO = true
    end)

    -- Watch Filter specific changes
    R.watch(function()
        Model.filters.hpfBypass:get()
        Model.filters.bpfIndex:get()
        
        local box = Model.getSelectedSignalBox()
        if box then local _ = box.frequency end
        
        dirty.filters = true
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
            -- Only update VFO if it's not already within the band or at default
            local current = Model.rx.VFO.activeValue:peek()
            if math.abs(current - freq) > 1 then
                Model.set("rx.VFO.activeValue", freq)
            end
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

    -- Update AGC/RF from hardware IF they changed elsewhere (e.g. twin auto-agc)
    if state.agcEnabled ~= nil then
        if state.agcEnabled ~= Model.rx.AGC.enabled:peek() then
            Model.rx.AGC.enabled:set(state.agcEnabled)
        end
    end
    if state.rfGainDB ~= nil then
        if math.abs(state.rfGainDB - Model.rx.RF.gainDB:peek()) > 0.5 then
            Model.rx.RF.gainDB:set(state.rfGainDB)
        end
    end

    -- Update Filter status from hardware (not usually needed but for consistency)
    if state.hpfBypass ~= nil then
        if state.hpfBypass ~= Model.filters.hpfBypass:peek() then
            Model.filters.hpfBypass:set(state.hpfBypass)
        end
    end
    if state.bpfIndex ~= nil then
        if state.bpfIndex ~= Model.filters.bpfIndex:peek() then
            Model.filters.bpfIndex:set(state.bpfIndex)
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
            
            commands.VFO = loFreq
            commands.tuningOffset = tuneHz
        end
        dirty.VFO = false
        anyDirty = true
    end

    if dirty.filters then
        local f = Model.filters
        local box = Model.getSelectedSignalBox()
        local freq = box and box.frequency or 14.2e6
        
        commands.filters = {
            hpfBypass = f.hpfBypass:peek(),
            bpfIndex = BpfCal.calculate(freq)
        }
        dirty.filters = false
        anyDirty = true
    end

    if dirty.volume then
        local vol = Model.rx.volume.DB:peek()
        local muted = Model.rx.volume.muted:peek()
        commands.volume = {
            DB = vol,
            muted = muted
        }
        dirty.volume = false
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
        local gain = Model.rx.RF.gainDB:peek()
        local atten = Model.rx.RF.attenuationDB:peek()
        commands.RF = {
            gainDB = gain,
            attenuationDB = atten
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
