--[[
    events.lua - SetBox-based Event Dispatch System (Unified Tag Architecture)

    Routes input events through SetBox rules based on namespaced tags:
    - event.*  : Event type (event.MouseDown-LEFT, event.KeyDown-H, etc.)
    - widget.* : Widget type and identity (widget.Button, widget.VFOControl, etc.)
    - state.*  : Widget/app state (state.Hovered, state.FreqEntryMode, etc.)
    - input.*  : Held inputs (input.SHIFT, input.CTRL, input.MouseLEFT, etc.)

    Events bubble from the deepest widget up through parents until handled.
    Handlers return true to stop bubbling, false to continue.
    Unhandled events are logged with full context.

    Usage:
        local events = require("Events")
        events.init()

        -- Register a handler
        events.registerHandler("freq_tune", function(event, widget)
            frequency = frequency + event.delta * 0.001
            return true  -- handled
        end)

        -- In update(), dispatch raw events:
        events.dispatch({
            type = events.Type.MOUSE_WHEEL,
            x = mouseX, y = mouseY,
            delta = wheelDelta,
            modifiers = {"input.SHIFT"},
        })
]]

local Events = {}

-- ============================================================================
-- Event Types
-- ============================================================================

Events.Type = {
    MOUSE_DOWN = "MouseDown",
    MOUSE_UP = "MouseUp",
    MOUSE_MOVE = "MouseMove",
    MOUSE_WHEEL = "MouseWheel",
    KEY_DOWN = "KeyDown",
    KEY_UP = "KeyUp",
    TEXT_INPUT = "TextInput",
    WINDOW_RESIZE = "WindowResize",
    WINDOW_MAPPED = "WindowMapped",
    WINDOW_UNMAPPED = "WindowUnmapped",
    FOCUS_IN = "FocusIn",
    FOCUS_OUT = "FocusOut",
}

-- ============================================================================
-- Widget Registry
-- ============================================================================

-- Registered widgets: {id -> {bounds, tags, parent, children, zIndex}}
Events.widgets = {}

-- Widget z-order for hit testing (higher = on top)
Events.nextZIndex = 1

-- Layout parent stack (for hierarchy inference)
Events._layoutParentStack = {}

-- Current layout parent
Events._currentLayoutParent = nil

-- ============================================================================
-- Mode Tags (global tags for application modes like FreqEntryMode)
-- ============================================================================

-- Active mode tags: {tagName -> true}
Events.modeTags = {}

-- ============================================================================
-- Handler Registry
-- ============================================================================

-- Named handlers: {name -> function(event, widget) -> handled}
Events.handlers = {}

-- ============================================================================
-- Public API - Initialization
-- ============================================================================

--- Initialize the event system
function Events.init()
    Events.widgets = {}
    Events.handlers = {}
    Events.modeTags = {}
    Events._layoutParentStack = {}
    Events._currentLayoutParent = nil
    Events.nextZIndex = 1

    -- Verify setbox is available for event resolution
    if not setbox then
    else
        -- Verify required functions exist
        if not setbox.setActiveTags then
        end
        if not setbox.getString then
        end
        -- Count rules to verify config loaded
        local ruleCount = 0
        if setbox.getRules then
            ruleCount = #setbox.getRules()
        end
    end
end

-- ============================================================================
-- Public API - Widget Registration
-- ============================================================================

--- Register a widget for event dispatch
-- @param id unique widget ID
-- @param bounds {x, y, w, h}
-- @param tags array of tags (e.g., {"Button", "Primary"})
-- @param explicitParent optional explicit parent widget ID (nil = infer from layout)
-- @param data optional widget-specific data (e.g., {min=0, max=100} for sliders)
function Events.registerWidget(id, bounds, tags, explicitParent, data)
    local parent = explicitParent or Events._currentLayoutParent

    Events.widgets[id] = {
        id = id,
        bounds = bounds or {x = 0, y = 0, w = 0, h = 0},
        tags = tags or {},
        parent = parent,
        children = {},
        zIndex = Events.nextZIndex,
        data = data or {},  -- Widget-specific data (min, max, property, etc.)
    }
    Events.nextZIndex = Events.nextZIndex + 1

    -- Register as child of parent
    if parent and Events.widgets[parent] then
        table.insert(Events.widgets[parent].children, id)
    end
end

--- Update widget bounds (call during draw if widget moved)
-- @param id widget ID
-- @param bounds {x, y, w, h}
function Events.updateWidgetBounds(id, bounds)
    local widget = Events.widgets[id]
    if widget then
        widget.bounds = bounds
    end
end

--- Unregister a widget
-- @param id widget ID
function Events.unregisterWidget(id)
    local widget = Events.widgets[id]
    if not widget then return end

    -- Remove from parent's children list
    if widget.parent and Events.widgets[widget.parent] then
        local parentChildren = Events.widgets[widget.parent].children
        for i, childId in ipairs(parentChildren) do
            if childId == id then
                table.remove(parentChildren, i)
                break
            end
        end
    end

    -- Unregister all children
    for _, childId in ipairs(widget.children) do
        Events.unregisterWidget(childId)
    end

    Events.widgets[id] = nil
end

--- Clear all widgets (call at start of frame in immediate-mode UI)
function Events.clearWidgets()
    Events.widgets = {}
    Events._layoutParentStack = {}
    Events._currentLayoutParent = nil
    Events.nextZIndex = 1
end

-- ============================================================================
-- Public API - Layout Parent Tracking
-- ============================================================================

--- Push a layout region as parent context
-- Call from layout.dock() to track hierarchy
-- @param regionId region identifier
function Events.pushLayoutParent(regionId)
    table.insert(Events._layoutParentStack, Events._currentLayoutParent)
    Events._currentLayoutParent = regionId
end

--- Pop layout parent context
-- Call from layout.endDock()
function Events.popLayoutParent()
    Events._currentLayoutParent = table.remove(Events._layoutParentStack) or nil
end

--- Get current layout parent
-- @return parent ID or nil
function Events.getCurrentLayoutParent()
    return Events._currentLayoutParent
end

-- ============================================================================
-- Public API - Mode Tag Management
-- ============================================================================

--- Add a mode tag (e.g., "FreqEntryMode")
-- Mode tags are included in all event tag resolution
-- @param tag tag name to add
function Events.addModeTag(tag)
    Events.modeTags[tag] = true
end

--- Remove a mode tag
-- @param tag tag name to remove
function Events.removeModeTag(tag)
    Events.modeTags[tag] = nil
end

--- Check if a mode tag is active
-- @param tag tag name to check
-- @return true if tag is active
function Events.hasModeTag(tag)
    return Events.modeTags[tag] == true
end

--- Get list of all active mode tags
-- @return array of active tag names
function Events.getModeTags()
    local tags = {}
    for tag, _ in pairs(Events.modeTags) do
        table.insert(tags, tag)
    end
    return tags
end

-- ============================================================================
-- Public API - Handler Registration
-- ============================================================================

--- Register a named event handler
-- @param name handler name (referenced in SetBox rules)
-- @param fn function(event, widget) -> boolean (true = handled)
function Events.registerHandler(name, fn)
    Events.handlers[name] = fn
end

--- Unregister a handler
-- @param name handler name
function Events.unregisterHandler(name)
    Events.handlers[name] = nil
end

-- ============================================================================
-- Public API - Event Dispatch
-- ============================================================================

-- Debug flag for event dispatch (disabled by default for performance)
Events.debugDispatch = false

--- Dispatch an event through the widget hierarchy
-- @param event {type, x, y, button, key, delta, modifiers, ...}
-- @return true if handled, false if bubbled to root unhandled
function Events.dispatch(event)
    if not event or not event.type then
        return false
    end

    -- Find widget under mouse (for mouse events)
    local targetWidget = nil
    if event.x and event.y then
        targetWidget = Events.getWidgetAt(event.x, event.y)
    end

    -- Bubble through widget hierarchy
    local currentWidget = targetWidget
    local handled = false
    
    if event.type == Events.Type.MOUSE_WHEEL or event.type == Events.Type.KEY_DOWN then
    end

    while true do
        -- Build tags for SetBox resolution
        local tags = Events._buildEventTags(event, currentWidget)

        -- Resolve handler and properties via SetBox
        local props = Events._resolveHandler(tags)

        if props and props.handler then
            local handler = Events.handlers[props.handler]
            if handler then
                local ok, result = pcall(handler, event, currentWidget, props)
                if ok and result == true then
                    handled = true
                    break
                end
                if not ok then
                end
            end
        end

        -- Bubble to parent
        if currentWidget and currentWidget.parent then
            currentWidget = Events.widgets[currentWidget.parent]
        else
            -- Reached root
            break
        end
    end

    -- If not handled by any widget, try global handlers (tags without widget tags)
    if not handled then
        local globalTags = Events._buildEventTags(event, nil)
        local props = Events._resolveHandler(globalTags)
        if props and props.handler and Events.handlers[props.handler] then
            local ok, result = pcall(Events.handlers[props.handler], event, nil, props)
            if ok and result == true then
                handled = true
            end
        end
    end

    -- No widget or global handled the event - try global unhandled handler
    if not handled then
        local unhandledTags = {"event.Unhandled"}
        Events._addModifierTags(event, unhandledTags)

        local props = Events._resolveHandler(unhandledTags)
        if props and props.handler and Events.handlers[props.handler] then
            pcall(Events.handlers[props.handler], event, targetWidget, props)
        end
    end

    return handled
end

--- Dispatch a keyboard event
-- Now uses the same bubbling logic as mouse events for consistency
function Events.dispatchKey(event)
    return Events.dispatch(event)
end

-- ============================================================================
-- Public API - Hit Testing
-- ============================================================================

--- Get the deepest widget at a point
-- @param x x coordinate
-- @param y y coordinate
-- @return widget table or nil
function Events.getWidgetAt(x, y)
    local best = nil
    local bestZ = -1

    for _, widget in pairs(Events.widgets) do
        local b = widget.bounds
        if x >= b.x and x < b.x + b.w and
           y >= b.y and y < b.y + b.h then
            if widget.zIndex > bestZ then
                best = widget
                bestZ = widget.zIndex
            end
        end
    end

    return best
end

--- Check if point is inside a widget
-- @param x x coordinate
-- @param y y coordinate
-- @param widgetId widget ID
-- @return boolean
function Events.isPointInWidget(x, y, widgetId)
    local widget = Events.widgets[widgetId]
    if not widget then return false end
    local b = widget.bounds
    return x >= b.x and x < b.x + b.w and y >= b.y and y < b.y + b.h
end

-- ============================================================================
-- Internal Functions
-- ============================================================================

--- Build tags array for event resolution (namespaced)
-- @param event the event
-- @param widget current widget (may be nil)
-- @return tags array
function Events._buildEventTags(event, widget)
    local tags = {}

    -- Build event tag: event.MouseDown-LEFT, event.KeyDown-H, event.MouseWheel, etc.
    local eventTag = "event." .. event.type
    if event.button then
        -- Mouse button events: event.MouseDown-LEFT, event.MouseUp-RIGHT, etc.
        eventTag = eventTag .. "-" .. event.button
    elseif event.key then
        -- Keyboard events: event.KeyDown-H, event.KeyUp-ESC, etc.
        eventTag = eventTag .. "-" .. event.key
    end
    table.insert(tags, eventTag)

    -- Add widget tags (already namespaced from widgets.lua)
    if widget and widget.tags then
        for _, tag in ipairs(widget.tags) do
            table.insert(tags, tag)
        end
    end

    -- Add mode tags (state.* namespace)
    for modeTag, _ in pairs(Events.modeTags) do
        table.insert(tags, modeTag)
    end

    -- Add modifier tags last (input.SHIFT, input.CTRL, input.ALT, plus held buttons)
    Events._addModifierTags(event, tags)

    return tags
end

--- Add modifier key tags to tags array
-- @param event the event
-- @param tags tags array to modify
function Events._addModifierTags(event, tags)
    if not event.modifiers then return end
    
    -- Handle both array of strings and map of tagName -> true
    for k, v in pairs(event.modifiers) do
        if type(k) == "number" then
            -- Array of strings
            table.insert(tags, v)
        elseif v == true then
            -- Map of tags (like activeTags table)
            table.insert(tags, k)
        end
    end
end

-- All possible modifier tags (for two-phase resolution)
-- Includes generic (SHIFT) and specific (LSHIFT, RSHIFT) variants
-- All use input.* namespace
local MODIFIER_TAGS = {
    -- Generic modifiers (derived from specific)
    ["input.SHIFT"] = true, ["input.CTRL"] = true, ["input.ALT"] = true,
    -- Specific modifier keys
    ["input.LSHIFT"] = true, ["input.RSHIFT"] = true,
    ["input.LCTRL"] = true, ["input.RCTRL"] = true,
    ["input.LALT"] = true, ["input.RALT"] = true,
    ["input.LGUI"] = true, ["input.RGUI"] = true,
    -- Mouse buttons (held buttons act as modifiers for motion)
    ["input.MouseLEFT"] = true, ["input.MouseMIDDLE"] = true, ["input.MouseRIGHT"] = true,
    -- Single letter keys held as modifiers (e.g., H for fine tune)
    ["input.H"] = true,
}

--- Query SetBox for all handler-related properties with given tags
-- @param tags tags array
-- @return table with handler name and all other properties, or nil if no handler
function Events._querySetBoxProperties(tags)
    if not setbox then
        if Events.debugDispatch then
        end
        return nil
    end

    local oldTags = setbox.getActiveTags and setbox.getActiveTags() or {}
    if setbox.setActiveTags then
        setbox.setActiveTags(tags)
    end

    -- Query all relevant properties for generic handlers
    local props = {}
    if setbox.has("handler") then props.handler = setbox.getString("handler") end
    if setbox.has("property") then props.property = setbox.getString("property") end
    if setbox.has("value") then props.value = setbox.get("value") end
    
    -- Linear step properties
    if setbox.has("step") then props.step = setbox.getNumber("step") end
    if setbox.has("step_ctrl") then props.step_ctrl = setbox.getNumber("step_ctrl") end
    if setbox.has("step_shift") then props.step_shift = setbox.getNumber("step_shift") end
    if setbox.has("step_ctrl_shift") then props.step_ctrl_shift = setbox.getNumber("step_ctrl_shift") end
    
    -- Logarithmic factor properties
    if setbox.has("factor") then props.factor = setbox.getNumber("factor") end
    if setbox.has("factor_ctrl") then props.factor_ctrl = setbox.getNumber("factor_ctrl") end
    if setbox.has("factor_shift") then props.factor_shift = setbox.getNumber("factor_shift") end
    if setbox.has("factor_ctrl_shift") then props.factor_ctrl_shift = setbox.getNumber("factor_ctrl_shift") end
    
    -- Range limits
    if setbox.has("min") then props.min = setbox.getNumber("min") end
    if setbox.has("max") then props.max = setbox.getNumber("max") end


    if setbox.setActiveTags then
        setbox.setActiveTags(oldTags)
    end

    return props.handler and props or nil
end

--- Resolve handler and properties via SetBox with two-phase matching
-- Phase 1: Try with full tags (most specific)
-- Phase 2: Try without modifiers (more general fallback)
-- @param tags tags array
-- @return table with handler and properties, or nil
function Events._resolveHandler(tags)
    -- Phase 1: Try with full tags (most specific)
    local props = Events._querySetBoxProperties(tags)
    if props then
        return props
    end

    -- Phase 2: Try without modifiers (more general)
    local generalTags = {}
    for _, tag in ipairs(tags) do
        if not MODIFIER_TAGS[tag] then
            table.insert(generalTags, tag)
        end
    end

    -- Only try general if we actually removed modifiers
    if #generalTags < #tags then
        props = Events._querySetBoxProperties(generalTags)
    end

    return props
end

--- Create event object from raw input data
-- Helper for main.lua to create properly formatted events
-- @param eventType Events.Type value
-- @param data additional event data
-- @return event table
function Events.createEvent(eventType, data)
    local event = {
        type = eventType,
        timestamp = os.clock(),
        handled = false,
    }

    -- Copy data fields
    if data then
        for k, v in pairs(data) do
            event[k] = v
        end
    end

    -- Build modifiers array (namespaced)
    -- Prefer values passed in data, fall back to global functions
    if not event.modifiers then
        event.modifiers = {}
        
        local hasShift = data and data.shift ~= nil and data.shift or (isShiftDown and isShiftDown())
        local hasCtrl = data and data.ctrl ~= nil and data.ctrl or (isCtrlDown and isCtrlDown())
        local hasAlt = data and data.alt ~= nil and data.alt or (isAltDown and isAltDown())

        if hasShift then table.insert(event.modifiers, "input.SHIFT") end
        if hasCtrl then table.insert(event.modifiers, "input.CTRL") end
        if hasAlt then table.insert(event.modifiers, "input.ALT") end
        
        -- Also check for held mouse buttons if passed in data
        if data and data.mouseLeftHeld then table.insert(event.modifiers, "input.MouseLEFT") end
        if data and data.mouseMiddleHeld then table.insert(event.modifiers, "input.MouseMIDDLE") end
        if data and data.mouseRightHeld then table.insert(event.modifiers, "input.MouseRIGHT") end
    end

    return event
end

return Events
