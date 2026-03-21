--[[
    Events.lua - Unified Event Type Definitions and Registry
    
    This module no longer handles rule-based dispatch via SetBox.
    It provides a central registry for event handlers and standardized
    event type constants.
]]

local Events = {}

Events.Type = {
    MOUSE_DOWN = "mouseButton",
    MOUSE_UP = "mouseButton",
    MOUSE_MOVE = "mouseMotion",
    MOUSE_WHEEL = "mouseWheel",
    KEY_DOWN = "key",
    KEY_UP = "key",
    TEXT_INPUT = "textInput"
}

-- Handler Registry for shared logic
Events.handlers = {}

function Events.registerHandler(name, fn)
    Events.handlers[name] = fn
end

function Events.getHandler(name)
    return Events.handlers[name]
end

function Events.init()
    Events.handlers = {}
end

-- Compatibility helpers for Main.lua
function Events.createEvent(eventType, data)
    local event = {
        type = eventType,
        timestamp = os.clock()
    }
    if data then
        for k, v in pairs(data) do event[k] = v end
    end
    return event
end

-- These are now NO-OPS as tags are no longer used for dispatch
function Events.addModeTag() end
function Events.removeModeTag() end
function Events.hasModeTag() return false end

return Events
