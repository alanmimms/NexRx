--[[
  NexRx Property Change Handlers

  This module provides a table-based dispatch mechanism for property changes.
  When a config property changes, the corresponding handler is called.

  This replaces C++ if-else chains with Lua table lookups, making the
  control plane more flexible and extensible.

  Usage:
    local handlers = require("property_handlers")
    handlers.dispatch("volume", 0.8)
]]

local PropertyHandlers = {}

-- Handler table: property name -> handler function
-- Each handler receives the new value
local handlers = {}

-- =============================================================================
-- Audio Handlers
-- =============================================================================

handlers.volume = function(value)
    if audio and audio.setVolume then
        audio.setVolume(value)
    end
end

handlers.muted = function(value)
    if audio and audio.setMuted then
        audio.setMuted(value)
    end
end

handlers.testToneEnabled = function(value)
    if audio and audio.setTestTone and config then
        audio.setTestTone(value, config.testToneFreq)
    end
end

handlers.testToneFreq = function(value)
    if audio and audio.setTestTone and config then
        audio.setTestTone(config.testToneEnabled, value)
    end
end

-- =============================================================================
-- Demodulator/Mode Handlers
-- =============================================================================

handlers.mode = function(value)
    -- value is a ModeChoice enum value (integer)
    -- Pass directly to C++ - no string conversion needed
    if rx and rx.setModeId then
        rx.setModeId(value)
    end
end

-- =============================================================================
-- Waterfall/Display Handlers
-- =============================================================================

handlers.colormap = function(value)
    -- value is a ColormapChoice enum, convert to name
    local colormapMap = {
        [0] = "viridis",
        [1] = "plasma",
        [2] = "inferno",
        [3] = "green",
        [4] = "blue",
    }
    local colormapName = colormapMap[value] or "viridis"

    -- Use Lua-defined colormaps if available
    if colormaps and colormaps[colormapName] and waterfall and waterfall.setColormapData then
        waterfall.setColormapData(colormaps[colormapName])
    elseif waterfall and waterfall.setColormap then
        waterfall.setColormap(colormapName)
    end
end

-- =============================================================================
-- Baseband Filter Handlers
-- =============================================================================

handlers.bandpassEnabled = function(value)
    if rx and rx.setBandpassEnabled then
        rx.setBandpassEnabled(value)
    end
end

handlers.bandpassCenter = function(value)
    if rx and rx.setBandpassCenter then
        rx.setBandpassCenter(value)
    end
end

handlers.bandpassWidth = function(value)
    if rx and rx.setBandpassWidth then
        rx.setBandpassWidth(value)
    end
end

handlers.bandpassTaps = function(value)
    if rx and rx.setBandpassTaps then
        rx.setBandpassTaps(math.floor(value))
    end
end

handlers.notchEnabled = function(value)
    if rx and rx.setNotchEnabled then
        rx.setNotchEnabled(value)
    end
end

handlers.notchCenter = function(value)
    if rx and rx.setNotchCenter then
        rx.setNotchCenter(value)
    end
end

handlers.notchWidth = function(value)
    if rx and rx.setNotchWidth then
        rx.setNotchWidth(value)
    end
end

-- =============================================================================
-- Hardware Handlers
-- =============================================================================

handlers.qsdOffsetK = function(value)
    if hw and hw.setQsdOffset then
        hw.setQsdOffset(value)
    end
end

handlers.rfAttenuation = function(value)
    if hw and hw.setAttenuation then
        hw.setAttenuation(value)
    end
end

-- =============================================================================
-- Dispatch Function
-- =============================================================================

--- Dispatch a property change to its handler
-- @param name Property name
-- @param value New value
-- @return true if handler was called, false if no handler
function PropertyHandlers.dispatch(name, value)
    local handler = handlers[name]
    if handler then
        local ok, err = pcall(handler, value)
        if not ok then
            print("[PropertyHandlers] Error in handler for '" .. name .. "': " .. tostring(err))
            return false
        end
        return true
    end
    return false
end

--- Register a custom handler
-- @param name Property name
-- @param handler Function to call with new value
function PropertyHandlers.register(name, handler)
    handlers[name] = handler
end

--- Get list of handled property names
function PropertyHandlers.getHandledProperties()
    local names = {}
    for name in pairs(handlers) do
        table.insert(names, name)
    end
    table.sort(names)
    return names
end

-- Make dispatch available globally for C++ to call
_G.onPropertyChange = function(name, value)
    PropertyHandlers.dispatch(name, value)
end

print("[property_handlers.lua] Property handlers loaded")

return PropertyHandlers
