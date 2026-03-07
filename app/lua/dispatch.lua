--[[
    dispatch.lua - Function Pointer Dispatch System

    Eliminates runtime conditionals by using function variables that are
    swapped when conditions change. Functions start as no-ops/fallbacks
    and are replaced with real implementations when subsystems become ready.

    Usage:
        local dispatch = require("dispatch")
        dispatch.init()

        -- In update():
        local spectrum = dispatch.getSpectrum()
        dispatch.updateWaterfall(spectrum)

        -- When hardware connects:
        dispatch.enableHardware()
]]

local Dispatch = {}

-- ============================================================================
-- Spectrum Source Functions
-- ============================================================================

-- Get spectrum data: returns array or nil (nil = use fallback)
Dispatch.getSpectrum = function()
    return nil  -- Default: no spectrum (triggers fallback)
end

-- Fallback spectrum generator (set by main.lua)
Dispatch.generateFallbackSpectrum = function()
    return {}  -- Default: empty array
end

-- ============================================================================
-- Waterfall Functions
-- ============================================================================

-- Add row to waterfall (no-op until waterfall initialized)
Dispatch.updateWaterfall = function(data) end

-- Render waterfall display (no-op until waterfall initialized)
Dispatch.renderWaterfall = function(x, y, w, h) end

-- Render spectrum analyzer (no-op until waterfall initialized)
Dispatch.renderSpectrum = function(data, x, y, w, h) end

-- ============================================================================
-- Hardware Control Functions
-- ============================================================================

-- Send VFO frequency to hardware (no-op if hardware not connected)
Dispatch.setVfo = function(freqHz) end

-- Set receiver active state
Dispatch.setRxActive = function(active) end

-- Set QSD offset (no-op if hardware not connected)
Dispatch.setQsdOffset = function(kHz) end

-- Set RF attenuation (no-op if hardware not connected)
Dispatch.setAttenuation = function(dB) end

-- ============================================================================
-- Implementation Functions (private)
-- ============================================================================

local function hwGetSpectrum()
    if not hw or not hw.isConnected() then
        return nil
    end
    local spectrum = hw.getSpectrum()
    if spectrum and #spectrum > 0 then
        return spectrum
    end
    return nil
end

local function hwSendVfo(freqHz)
    if hw and hw.isConnected() then
        -- Some hardware uses rx.setVfo, some uses hw.setVfo
        -- In our C++ app, rx.setVfo is the one that sends to twin
        if rx and rx.setVfo then
            rx.setVfo(freqHz)
        end
    end
end

local function hwSetRxActive(active)
    if hw and hw.isConnected() then
        -- Tell hardware to start/stop stream if supported
        if active then
            if hw.startStream then hw.startStream() end
        else
            if hw.stopStream then hw.stopStream() end
        end
    end
end

local function hwSetQsdOffset(kHz)
    if hw and hw.isConnected() then
        hw.setQsdOffset(kHz)
    end
end

local function hwSetAttenuation(dB)
    if hw and hw.isConnected() then
        hw.setAttenuation(dB)
    end
end

local function waterfallAddRow(data)
    if waterfall and waterfall.isInitialized() then
        waterfall.addRow(data)
    end
end

local function waterfallRender(x, y, w, h)
    if waterfall and waterfall.isInitialized() then
        waterfall.render(x, y, w, h)
    end
end

local function waterfallRenderSpectrum(data, x, y, w, h)
    if waterfall and waterfall.isInitialized() then
        waterfall.renderSpectrum(data, x, y, w, h)
    end
end

-- ============================================================================
-- Public API
-- ============================================================================

--- Initialize dispatch system
-- Call after subsystems are created but may not be fully ready
function Dispatch.init()
end

--- Enable waterfall functions
-- Call when waterfall.isInitialized() returns true
function Dispatch.enableWaterfall()
    Dispatch.updateWaterfall = waterfallAddRow
    Dispatch.renderWaterfall = waterfallRender
    Dispatch.renderSpectrum = waterfallRenderSpectrum
end

--- Disable waterfall functions (revert to no-op)
function Dispatch.disableWaterfall()
    Dispatch.updateWaterfall = function(data) end
    Dispatch.renderWaterfall = function(x, y, w, h) end
    Dispatch.renderSpectrum = function(data, x, y, w, h) end
end

--- Enable hardware functions
-- Call when hardware connects
function Dispatch.enableHardware()
    Dispatch.getSpectrum = hwGetSpectrum
    Dispatch.setVfo = hwSendVfo
    Dispatch.setRxActive = hwSetRxActive
    Dispatch.setQsdOffset = hwSetQsdOffset
    Dispatch.setAttenuation = hwSetAttenuation
end

--- Disable hardware functions (revert to no-op/fallback)
-- Call when hardware disconnects
function Dispatch.disableHardware()
    Dispatch.getSpectrum = function() return nil end
    Dispatch.setVfo = function(freqHz) end
    Dispatch.setRxActive = function(active) end
    Dispatch.setQsdOffset = function(kHz) end
    Dispatch.setAttenuation = function(dB) end
end

--- Set fallback spectrum generator function
-- @param fn function that returns spectrum array
function Dispatch.setFallbackGenerator(fn)
    Dispatch.generateFallbackSpectrum = fn
end

--- Get spectrum with automatic fallback
-- Tries hardware first, falls back to generator if nil
-- @return spectrum array
function Dispatch.getSpectrumWithFallback()
    local spectrum = Dispatch.getSpectrum()
    if spectrum and #spectrum > 0 then
        return spectrum
    end
    return Dispatch.generateFallbackSpectrum()
end

--- Check if hardware is currently enabled
-- @return boolean
function Dispatch.isHardwareEnabled()
    return Dispatch.getSpectrum == hwGetSpectrum
end

--- Check if waterfall is currently enabled
-- @return boolean
function Dispatch.isWaterfallEnabled()
    return Dispatch.updateWaterfall == waterfallAddRow
end

return Dispatch
