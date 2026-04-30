--[[
    Hardware.lua - Unified Hardware Abstraction Layer (HAL)
    
    Provides a batched interface to communicate with the STM32/Twin
    hardware. Ensures all acronyms are strictly uppercase (VFO, AGC, QSD).
]]

local Hardware = {}
_G.Hardware = Hardware

-- ============================================================================
-- State & Initialization
-- ============================================================================

local hwEnabled = false
local waterfallEnabled = false

-- Command deduplication state
local lastSent = {}

local function shouldSend(key, value)
    local v = lastSent[key]
    -- For tables, we need to deep compare if we want perfect deduplication, 
    -- but for now we'll handle sub-keys individually in sync().
    if type(value) ~= "table" then
        if v == value then return false end
        lastSent[key] = value
        return true
    end
    return true -- Always send tables for now, but sync() will check fields
end

-- Force resend of all state (call on reconnect)
function Hardware.invalidateState()
    lastSent = {}
end

function Hardware.init()
    -- Subsystems initialization
end

-- ============================================================================
-- Round-based Synchronization
-- ============================================================================

--- Synchronize Model changes to Hardware in a single transaction
-- @param commands Table of namespaced commands to execute
function Hardware.sync(commands)
    if not hwEnabled or not commands then return end

    if commands.VFO then
        if shouldSend("VFO.freq", commands.VFO) then
            print("[Hardware] Syncing VFO: " .. tostring(commands.VFO) .. " (rx=" .. tostring(rx) .. ")")
            if rx and rx.setVFO then
                rx.setVFO(commands.VFO)
            else
                print("[Hardware] ERROR: rx.setVFO not found!")
            end
        end
    end

    if commands.tuningOffset then
        if shouldSend("VFO.tuning", commands.tuningOffset) then
            if rx and rx.setTuningOffset then
                rx.setTuningOffset(commands.tuningOffset)
            end
        end
    end

    -- RF Filter Synchronization
    if commands.filters then
        local f = commands.filters
        if hw then
            if f.hpfBypass ~= nil and shouldSend("hpf.bypass", f.hpfBypass) and hw.setHpfBypass then
                hw.setHpfBypass(f.hpfBypass)
            end
            if f.bpfIndex ~= nil and shouldSend("bpf.index", f.bpfIndex) and hw.setBpfIndex then
                hw.setBpfIndex(f.bpfIndex)
            end
        end
    end

    -- AGC Synchronization
    if commands.AGC then
        local a = commands.AGC
        if hw then
            if a.enabled ~= nil and shouldSend("agc.en", a.enabled) and hw.setAGCEnabled then hw.setAGCEnabled(a.enabled) end
            if a.mode ~= nil and shouldSend("agc.mode", a.mode) and hw.setAGCMode then hw.setAGCMode(a.mode) end
        end
    end
    
    -- RF Control
    if commands.RF then
        local r = commands.RF
        if hw then
            if r.gainDB ~= nil and shouldSend("rf.gain", r.gainDB) and hw.setRFGain then hw.setRFGain(r.gainDB) end
            if r.attenuationDB ~= nil and shouldSend("rf.atten", r.attenuationDB) and hw.setRFAttenuation then hw.setRFAttenuation(r.attenuationDB) end
        end
    end

    -- QSD Control
    if commands.QSD then
        if hw and shouldSend("qsd.offset", commands.QSD.offsetK) and hw.setQSDOffset then
            hw.setQSDOffset(commands.QSD.offsetK)
        end
    end

    -- Volume Control
    if commands.volume then
        local v = commands.volume
        if audio then
            if v.DB ~= nil and shouldSend("audio.vol", v.DB) and audio.setVolume then
                audio.setVolume(v.DB)
            end
            if v.muted ~= nil and shouldSend("audio.mute", v.muted) and audio.setMuted then
                audio.setMuted(v.muted)
            end
        end
    end
end

-- ============================================================================
-- Direct Getters (Immediate)
-- ============================================================================

function Hardware.getSpectrum()
    if not hwEnabled then return nil end
    if not hw or not hw.isConnected() then return nil end
    
    local spectrum = hw.getSpectrum()
    if spectrum and #spectrum > 0 then
        return spectrum
    end
    
    return nil
end

function Hardware.getState()
    if not hwEnabled or not hw or not hw.isConnected() then return nil end
    if hw.getState then
        local s = hw.getState()
        -- if s then print("[Hardware] Got state map, keys=" .. #s) end
        return s
    end
    return nil
end

-- ============================================================================
-- Waterfall / UI Support
-- ============================================================================

function Hardware.updateWaterfall(data)
    if waterfall and waterfall.addRow then
        waterfall.addRow(data)
    end
end

function Hardware.renderWaterfall(x, y, w, h, zoom, center)
    if waterfall and waterfall.render then
        waterfall.render(x, y, w, h, zoom or 1.0, center or 0.5)
    end
end

function Hardware.renderSpectrum(data, x, y, w, h)
    if waterfall and waterfall.renderSpectrum then
        if not data or #data == 0 then
            if not hwEnabled then
                -- Only generate dummy spectrum if hardware is NOT enabled
                data = {}
                for i = 1, 1024 do data[i] = -100 + 20 * math.sin(i / 50) + 5 * math.random() end
            else
                -- If hardware is enabled but data is empty, just return early
                return
            end
        end
        waterfall.renderSpectrum(data, x, y, w, h)
    end
end

-- ============================================================================
-- Lifecycle Management
-- ============================================================================

function Hardware.enableHardware() hwEnabled = true end
function Hardware.disableHardware() hwEnabled = false end
function Hardware.enableWaterfall() waterfallEnabled = true end
function Hardware.disableWaterfall() waterfallEnabled = false end

function Hardware.isHardwareEnabled() return hwEnabled end
function Hardware.isWaterfallEnabled() return waterfallEnabled end

-- Generic VFO Setter (used for direct interaction if needed)
function Hardware.setVFO(freqHz)
    if rx and rx.setVFO then rx.setVFO(freqHz) end
end

return Hardware
