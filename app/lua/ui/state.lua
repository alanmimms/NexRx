--[[
  UI State Management for Immediate-Mode GUI

  Tracks hot (hovered), active (pressed), and focus state for widgets.
  Uses string IDs to identify widgets across frames.
]]

local state = {}

-- Current frame state
state.hot = nil        -- Widget ID under mouse
state.active = nil     -- Widget ID being pressed/dragged
state.focus = nil      -- Widget ID with keyboard focus

-- Previous frame state (for detecting changes)
state.prevHot = nil
state.prevActive = nil

-- Mouse state
state.mouseX = 0
state.mouseY = 0
state.mouseDown = false
state.mouseClicked = false   -- True for one frame when clicked
state.mouseReleased = false  -- True for one frame when released
state.mouseWheel = 0

-- Keyboard state
state.keyPressed = nil   -- Key code pressed this frame
state.keyChar = nil      -- Character typed this frame

-- Widget state storage (for multi-frame state like text input)
state.widgetData = {}

-- Begin a new frame - call at start of update/draw
function state.beginFrame()
    state.prevHot = state.hot
    state.prevActive = state.active
    state.hot = nil  -- Will be set by widgets as they're drawn

    -- Get input state from C++ host
    state.mouseX, state.mouseY = getMousePos()
    local wasDown = state.mouseDown
    state.mouseDown = isMouseDown(0)
    state.mouseClicked = state.mouseDown and not wasDown
    state.mouseReleased = not state.mouseDown and wasDown
    state.mouseWheel = getMouseWheel()

    -- Clear single-frame state
    state.keyPressed = nil
    state.keyChar = nil
end

-- End frame - finalize state
function state.endFrame()
    -- If mouse released, clear active
    if state.mouseReleased then
        state.active = nil
    end
end

-- Check if point is inside rectangle
function state.pointInRect(px, py, x, y, w, h)
    return px >= x and px < x + w and py >= y and py < y + h
end

-- Set hot widget (called by widgets during draw)
function state.setHot(id)
    -- Only set hot if no widget is active, or this widget is active
    if state.active == nil or state.active == id then
        state.hot = id
    end
end

-- Set active widget (called when widget is clicked)
function state.setActive(id)
    state.active = id
end

-- Set focused widget (for keyboard input)
function state.setFocus(id)
    state.focus = id
end

-- Clear focus
function state.clearFocus()
    state.focus = nil
end

-- Check if widget is hot (hovered)
function state.isHot(id)
    return state.hot == id
end

-- Check if widget is active (being pressed/dragged)
function state.isActive(id)
    return state.active == id
end

-- Check if widget has focus
function state.hasFocus(id)
    return state.focus == id
end

-- Check if widget was just clicked (hot + mouse released on it)
function state.wasClicked(id)
    return state.hot == id and state.mouseReleased and state.active == id
end

-- Get persistent widget data
function state.getData(id, key, default)
    if not state.widgetData[id] then
        return default
    end
    local val = state.widgetData[id][key]
    if val == nil then
        return default
    end
    return val
end

-- Set persistent widget data
function state.setData(id, key, value)
    if not state.widgetData[id] then
        state.widgetData[id] = {}
    end
    state.widgetData[id][key] = value
end

return state
