--[[
    events.lua - SetBox-based Event Dispatch System

    Routes input events through SetBox rules based on tags for:
    - Event type (MouseDown, KeyDown, etc.)
    - Widget under mouse
    - Modifier keys (Shift, Ctrl, Alt)

    Events bubble from the deepest widget up through parents until handled.
    Handlers return true to stop bubbling, false to continue.
    Unhandled events are logged with full context.

    Usage:
        local events = require("events")
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
            modifiers = {"Shift"},
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

    -- Default unhandled handler is registered in main.lua with enhanced logging

    print("[Events] Event dispatch system initialized")
end

-- ============================================================================
-- Public API - Widget Registration
-- ============================================================================

--- Register a widget for event dispatch
-- @param id unique widget ID
-- @param bounds {x, y, w, h}
-- @param tags array of tags (e.g., {"Button", "Primary"})
-- @param explicitParent optional explicit parent widget ID (nil = infer from layout)
function Events.registerWidget(id, bounds, tags, explicitParent)
    local parent = explicitParent or Events._currentLayoutParent

    Events.widgets[id] = {
        id = id,
        bounds = bounds or {x = 0, y = 0, w = 0, h = 0},
        tags = tags or {},
        parent = parent,
        children = {},
        zIndex = Events.nextZIndex,
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
    while true do
        -- Build tags for SetBox resolution
        local tags = Events._buildEventTags(event, currentWidget)

        -- Resolve handler via SetBox
        local handlerName = Events._resolveHandler(tags)

        if handlerName then
            local handler = Events.handlers[handlerName]
            if handler then
                local ok, result = pcall(handler, event, currentWidget)
                if ok and result == true then
                    -- Event handled, stop bubbling
                    return true
                end
                -- Handler returned false or errored, continue bubbling
            end
        end

        -- Bubble to parent
        if currentWidget and currentWidget.parent then
            currentWidget = Events.widgets[currentWidget.parent]
        else
            -- Reached root, try unhandled handler
            break
        end
    end

    -- No widget handled the event - try global unhandled handler
    local unhandledTags = {"Event", event.type, "Unhandled"}
    Events._addModifierTags(event, unhandledTags)

    local handlerName = Events._resolveHandler(unhandledTags)
    if handlerName and Events.handlers[handlerName] then
        pcall(Events.handlers[handlerName], event, nil)
    end

    return false
end

--- Dispatch a keyboard event (not position-dependent)
-- @param event {type, key, scancode, modifiers, ...}
-- @return true if handled
function Events.dispatchKey(event)
    if not event or not event.type then
        return false
    end

    -- Build tags: {"Event", event.type, key_name, ...mode_tags, ...modifiers}
    local tags = {"Event", event.type}

    -- Add key name as tag (e.g., "Escape", "Enter", "F")
    if event.key then
        table.insert(tags, event.key)
    end

    -- Add mode tags (e.g., FreqEntryMode)
    for modeTag, _ in pairs(Events.modeTags) do
        table.insert(tags, modeTag)
    end

    -- Add modifier tags
    Events._addModifierTags(event, tags)

    -- Try to resolve handler via SetBox
    local handlerName = Events._resolveHandler(tags)
    if handlerName and Events.handlers[handlerName] then
        local ok, result = pcall(Events.handlers[handlerName], event, nil)
        if ok and result == true then
            return true
        end
    end

    -- If not handled, try with "Unhandled" tag
    table.insert(tags, "Unhandled")
    handlerName = Events._resolveHandler(tags)
    if handlerName and Events.handlers[handlerName] then
        pcall(Events.handlers[handlerName], event, nil)
    end

    return false
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

--- Build tags array for event resolution
-- @param event the event
-- @param widget current widget (may be nil)
-- @return tags array
function Events._buildEventTags(event, widget)
    local tags = {"Event", event.type}

    -- Add key name for keyboard events (e.g., "Escape", "Enter", "F")
    if event.key then
        table.insert(tags, event.key)
    end

    -- Add widget tags
    if widget and widget.tags then
        for _, tag in ipairs(widget.tags) do
            table.insert(tags, tag)
        end
    end

    -- Add mode tags (e.g., FreqEntryMode)
    for modeTag, _ in pairs(Events.modeTags) do
        table.insert(tags, modeTag)
    end

    -- Add modifier tags (Shift, Ctrl, Alt)
    Events._addModifierTags(event, tags)

    return tags
end

--- Add modifier key tags to tags array
-- @param event the event
-- @param tags tags array to modify
function Events._addModifierTags(event, tags)
    if not event.modifiers then return end
    for _, mod in ipairs(event.modifiers) do
        table.insert(tags, mod)
    end
end

--- Resolve handler name via SetBox
-- @param tags tags array
-- @return handler name string or nil
function Events._resolveHandler(tags)
    if not setbox then
        return nil
    end

    -- Save current tags, set event tags, resolve, restore
    local oldTags = setbox.getActiveTags and setbox.getActiveTags() or {}

    -- Set event-specific tags temporarily
    if setbox.setActiveTags then
        setbox.setActiveTags(tags)
    end

    -- Get handler property
    local handler = nil
    if setbox.getString then
        handler = setbox.getString("handler", nil)
    elseif setbox.get then
        handler = setbox.get("handler")
    end

    -- Restore previous tags
    if setbox.setActiveTags then
        setbox.setActiveTags(oldTags)
    end

    return handler
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

    -- Build modifiers array from C++ functions if available
    if not event.modifiers then
        event.modifiers = {}
        if isShiftDown and isShiftDown() then
            table.insert(event.modifiers, "Shift")
        end
        if isCtrlDown and isCtrlDown() then
            table.insert(event.modifiers, "Ctrl")
        end
        if isAltDown and isAltDown() then
            table.insert(event.modifiers, "Alt")
        end
    end

    return event
end

return Events
