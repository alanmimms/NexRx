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

    -- Verify setbox is available for event resolution
    if not setbox then
        print("[Events] WARNING: setbox global not available - event handlers will not resolve!")
    else
        -- Verify required functions exist
        if not setbox.setActiveTags then
            print("[Events] WARNING: setbox.setActiveTags not available")
        end
        if not setbox.getString then
            print("[Events] WARNING: setbox.getString not available")
        end
        -- Count rules to verify config loaded
        local ruleCount = 0
        if setbox.getRules then
            ruleCount = #setbox.getRules()
        end
        print(string.format("[Events] Event dispatch system initialized (setbox OK, %d rules)", ruleCount))
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

-- Debug flag for event dispatch (set to true to trace tag matching)
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

    -- Debug: show what widget was hit (only for click events, not motion)
    local showDebug = Events.debugDispatch and
        (event.type == "MouseDown" or event.type == "MouseUp")

    if showDebug then
        local widgetName = targetWidget and targetWidget.id or "none"
        local widgetTagsStr = targetWidget and table.concat(targetWidget.tags, ",") or ""
        print(string.format("[Events] %s widget=%s tags={%s}",
            event.type, widgetName, widgetTagsStr))
    end

    -- Bubble through widget hierarchy
    local currentWidget = targetWidget
    local firstBubble = true
    while true do
        -- Build tags for SetBox resolution
        local tags = Events._buildEventTags(event, currentWidget)

        -- Resolve handler and properties via SetBox
        local props = Events._resolveHandler(tags)

        -- Debug: show resolution result (only on first bubble for target widget)
        if showDebug and firstBubble then
            if props and props.handler then
                print(string.format("[Events] -> handler=%s", props.handler))
            else
                print("[Events] -> no handler")
            end
            firstBubble = false
        end

        if props and props.handler then
            local handler = Events.handlers[props.handler]
            if handler then
                local ok, result = pcall(handler, event, currentWidget, props)
                if ok and result == true then
                    -- Event handled, stop bubbling
                    return true
                end
                -- Handler returned false or errored, continue bubbling
                if not ok then
                    print(string.format("[Events] Handler '%s' error: %s", props.handler, result))
                end
            else
                -- Handler name resolved but not registered
                if Events.debugDispatch then
                    print(string.format("[Events DEBUG] Handler '%s' not registered!", props.handler))
                end
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

    local props = Events._resolveHandler(unhandledTags)
    if props and props.handler and Events.handlers[props.handler] then
        -- Pass the original target widget so unhandled handler can log it
        pcall(Events.handlers[props.handler], event, targetWidget, props)
    end

    return false
end

--- Dispatch a keyboard event
-- Uses same tag-building as mouse events for consistency
-- @param event {type, key, scancode, modifiers, x, y, ...}
-- @return true if handled
function Events.dispatchKey(event)
    if not event or not event.type then
        return false
    end

    -- Find widget under cursor (same as mouse events)
    local widget = nil
    if event.x and event.y then
        widget = Events.getWidgetAt(event.x, event.y)
    end

    -- Build tags using common function (includes widget tags, mode tags, modifiers)
    local tags = Events._buildEventTags(event, widget)

    -- Debug output for keyboard events
    if Events.debugDispatch and event.type == "KeyDown" then
        local widgetName = widget and widget.id or "none"
        print(string.format("[Events] %s key=%s widget=%s tags={%s}",
            event.type, event.key or "?", widgetName, table.concat(tags, ",")))
    end

    -- Try to resolve handler and properties via SetBox
    local props = Events._resolveHandler(tags)
    if props and props.handler and Events.handlers[props.handler] then
        if Events.debugDispatch and event.type == "KeyDown" then
            print(string.format("[Events] -> handler=%s", props.handler))
        end
        local ok, result = pcall(Events.handlers[props.handler], event, widget, props)
        if ok and result == true then
            return true
        end
    elseif Events.debugDispatch and event.type == "KeyDown" then
        print("[Events] -> no handler")
    end

    -- If not handled, try with "Unhandled" tag
    table.insert(tags, "Unhandled")
    props = Events._resolveHandler(tags)
    if props and props.handler and Events.handlers[props.handler] then
        pcall(Events.handlers[props.handler], event, widget, props)
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

    -- Add button for click/release events (e.g., "Left", "Right", "Middle")
    if event.button then
        table.insert(tags, event.button)
    end

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

    -- Add modifier tags last (Shift, Ctrl, Alt, plus held buttons for motion)
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

-- All possible modifier tags (for two-phase resolution)
local MODIFIER_TAGS = {
    Shift = true, Ctrl = true, Alt = true,
    Left = true, Middle = true, Right = true  -- Held buttons for motion
}

--- Query SetBox for all handler-related properties with given tags
-- @param tags tags array
-- @return table with handler name and all other properties, or nil if no handler
function Events._querySetBoxProperties(tags)
    if not setbox then
        if Events.debugDispatch then
            print("[Events DEBUG] setbox is nil!")
        end
        return nil
    end

    local oldTags = setbox.getActiveTags and setbox.getActiveTags() or {}
    if setbox.setActiveTags then
        setbox.setActiveTags(tags)
    end

    -- Query all relevant properties for generic handlers
    local props = {
        handler = setbox.getString and setbox.getString("handler", nil) or setbox.get("handler"),
        property = setbox.getString and setbox.getString("property", nil),
        value = setbox.get and setbox.get("value"),
        -- Linear step properties
        step = setbox.getNumber and setbox.getNumber("step", nil),
        step_ctrl = setbox.getNumber and setbox.getNumber("step_ctrl", nil),
        step_shift = setbox.getNumber and setbox.getNumber("step_shift", nil),
        step_ctrl_shift = setbox.getNumber and setbox.getNumber("step_ctrl_shift", nil),
        -- Logarithmic factor properties
        factor = setbox.getNumber and setbox.getNumber("factor", nil),
        factor_ctrl = setbox.getNumber and setbox.getNumber("factor_ctrl", nil),
        factor_shift = setbox.getNumber and setbox.getNumber("factor_shift", nil),
        factor_ctrl_shift = setbox.getNumber and setbox.getNumber("factor_ctrl_shift", nil),
        -- Range limits
        min = setbox.getNumber and setbox.getNumber("min", nil),
        max = setbox.getNumber and setbox.getNumber("max", nil),
    }


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
