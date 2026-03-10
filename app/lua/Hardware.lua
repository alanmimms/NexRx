--[[
    Hardware.lua - Unified Hardware Abstraction Layer (HAL)
    
    Provides a batched interface to communicate with the STM32/Twin
    hardware. Ensures all acronyms are strictly uppercase (VFO, AGC, QSD).
]]

local Hardware = {}

-- ============================================================================
-- State & Initialization
-- ============================================================================

local hwEnabled = false
local waterfallEnabled = false

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

    -- VFO Synchronization
    if commands.VFO then
        print("[Hardware] Syncing VFO: " .. tostring(commands.VFO) .. " (rx=" .. tostring(rx) .. ")")
        if rx and rx.setVFO then
            rx.setVFO(commands.VFO)
        else
            print("[Hardware] ERROR: rx.setVFO not found!")
        end
    end

    -- Preselector Synchronization
    if commands.preselector then
        local p = commands.preselector
        if hw then
            if p.L ~= nil and hw.setPreselectorInd then
                hw.setPreselectorInd(p.L and 1 or 0)
            end
            if p.capMask ~= nil and hw.setPreselectorCap then
                hw.setPreselectorCap(p.capMask)
            end
        end
    end

    -- AGC Synchronization
    if commands.AGC then
        local a = commands.AGC
        if hw then
            if a.enabled ~= nil and hw.setAGCEnabled then hw.setAGCEnabled(a.enabled) end
            if a.mode ~= nil and hw.setAGCMode then hw.setAGCMode(a.mode) end
        end
    end
    
    -- RF Control
    if commands.RF then
        local r = commands.RF
        if hw then
            if r.gainDB ~= nil and hw.setRFGain then hw.setRFGain(r.gainDB) end
            if r.attenuationDB ~= nil and hw.setRFAttenuation then hw.setRFAttenuation(r.attenuationDB) end
        end
    end

    -- QSD Control
    if commands.QSD then
        if hw and hw.setQSDOffset then
            hw.setQSDOffset(commands.QSD.offsetK)
        end
    end

    -- Volume Control
    if commands.volume then
        local v = commands.volume
        if audio and audio.setVolume then
            audio.setVolume(v.DB)
        end
        if audio and audio.setMuted then
            audio.setMuted(v.muted)
        end
    end
end

-- ============================================================================
-- Direct Getters (Immediate)
-- ============================================================================

function Hardware.getSpectrum()
    if not hwEnabled or not hw or not hw.isConnected() then return nil end
    local spectrum = hw.getSpectrum()
    if spectrum and #spectrum > 0 then return spectrum end
    return nil
end

-- ============================================================================
-- Waterfall / UI Support
-- ============================================================================

function Hardware.updateWaterfall(data)
    if waterfallEnabled and waterfall and waterfall.addRow then
        waterfall.addRow(data)
    end
end

function Hardware.renderWaterfall(x, y, w, h)
    if waterfallEnabled and waterfall and waterfall.render then
        waterfall.render(x, y, w, h)
    end
end

function Hardware.renderSpectrum(data, x, y, w, h)
    if waterfallEnabled and waterfall and waterfall.renderSpectrum then
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
