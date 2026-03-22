--[[
    Events.lua - Unified Event Type Definitions and Registry
    
    This module provides a central registry for event handlers and 
    standardized event type constants. It also implements dispatch
    via the SetBox rule system.
]]

local setbox = require("SetBox")

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

function Events.addModeTag(tag)
    setbox.addTag(tag)
end

function Events.removeModeTag(tag)
    setbox.removeTag(tag)
end

function Events.hasModeTag(tag)
    return setbox.hasTag(tag)
end

--- Global Dispatcher
-- Uses SetBox to find a handler based on current active tags + transient event tags.
function Events.dispatch(event, widget)
    -- 1. Identify transient tags for this event
    local transientTags = {}
    
    if event.type == "key" then
        local suffix = event.isDown and "KeyDown-" or "KeyUp-"
        table.insert(transientTags, "event." .. suffix .. tostring(event.key))
    elseif event.type == "textInput" then
        table.insert(transientTags, "event.TextInput")
    elseif event.type == "mouseButton" then
        local suffix = event.isDown and "MouseDown-" or "MouseUp-"
        table.insert(transientTags, "event." .. suffix .. tostring(event.button))
    elseif event.type == "mouseMotion" then
        table.insert(transientTags, "event.MouseMove")
    elseif event.type == "mouseWheel" then
        table.insert(transientTags, "event.MouseWheel")
    end
    
    -- 2. Add transient tags to SetBox
    for _, tag in ipairs(transientTags) do
        setbox.addTag(tag)
    end
    
    -- 3. Resolve handler from SetBox
    -- We use the provided widget's context if available, otherwise global
    local ctx = widget and widget.lwc or setbox
    local handlerName = ctx:optString("handler")
    
    local handled = false
    if handlerName and handlerName ~= "noop" then
        local handlerFn = Events.handlers[handlerName]
        if handlerFn then
            -- Note: We pass the resolved properties to the handler as the 3rd arg
            local props = ctx:resolve()
            handled = handlerFn(event, widget, props)
        else
            print("[Events] Warning: Handler '" .. handlerName .. "' not registered.")
        end
    end
    
    -- 4. Cleanup transient tags
    for _, tag in ipairs(transientTags) do
        setbox.removeTag(tag)
    end
    
    return handled
end

return Events
