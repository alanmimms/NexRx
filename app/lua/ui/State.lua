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

-- Global Mouse state (Screen space)
state.globalMouseX = 0
state.globalMouseY = 0

-- Local Mouse state (Relative to current widget origin)
state.mouseX = 0
state.mouseY = 0

state.mouseDown = false
state.mouseClicked = false   -- True for one frame when clicked
state.mouseReleased = false  -- True for one frame when released
state.mouseWheel = 0

-- Coordinate transformation stack
local offsetStack = {}
local currentOffsetX = 0
local currentOffsetY = 0

function state.pushOffset(x, y)
    table.insert(offsetStack, {x = currentOffsetX, y = currentOffsetY})
    currentOffsetX = currentOffsetX + x
    currentOffsetY = currentOffsetY + y
    state.mouseX = state.globalMouseX - currentOffsetX
    state.mouseY = state.globalMouseY - currentOffsetY
end

function state.popOffset()
    local old = table.remove(offsetStack)
    if old then
        currentOffsetX = old.x
        currentOffsetY = old.y
        state.mouseX = state.globalMouseX - currentOffsetX
        state.mouseY = state.globalMouseY - currentOffsetY
    end
end

function state.getOffset()
    return currentOffsetX, currentOffsetY
end

function state.setOffset(x, y)
    currentOffsetX = x or 0
    currentOffsetY = y or 0
    state.mouseX = state.globalMouseX - currentOffsetX
    state.mouseY = state.globalMouseY - currentOffsetY
end

-- Keyboard state
state.keyPressed = nil   -- Key code pressed this frame
state.keyChar = nil      -- Character typed this frame

-- Widget state storage (for multi-frame state like text input)
state.widgetData = {}

local eventsModule = nil

-- Begin a new frame - call at start of update/draw
function state.setEventsModule(ev)
    eventsModule = ev
end

function state.registerWidget(id, bounds, tags, data)
    if eventsModule and eventsModule.registerWidget then
        -- Convert local bounds to global using current offset
        local globalBounds = {
            x = bounds.x + currentOffsetX,
            y = bounds.y + currentOffsetY,
            w = bounds.w,
            h = bounds.h
        }
        eventsModule.registerWidget(id, globalBounds, tags, nil, data)
    end
end

function state.beginFrame(data)
    state.prevHot = state.hot
    state.prevActive = state.active
    state.hot = nil
    
    if eventsModule and eventsModule.clearWidgets then
        eventsModule.clearWidgets()
    end

    -- Reset offsets
    offsetStack = {}
    currentOffsetX = 0
    currentOffsetY = 0

    -- Get input state from C++ host or provided data
    if data then
        state.globalMouseX = data.mouseX or state.globalMouseX
        state.globalMouseY = data.mouseY or state.globalMouseY
        state.mouseDown = (data.mouseDown ~= nil) and data.mouseDown or state.mouseDown
        state.mouseClicked = (data.mouseClicked ~= nil) and data.mouseClicked or false
        state.mouseReleased = (data.mouseReleased ~= nil) and data.mouseReleased or false
        state.mouseWheel = data.mouseWheel or 0
    else
        local mx, my = getMousePos()
        state.globalMouseX = mx or 0
        state.globalMouseY = my or 0
        state.mouseDown = isMouseDown(0)
        state.mouseClicked = isMouseClicked(0)
        state.mouseReleased = isMouseReleased(0)
        state.mouseWheel = getMouseWheel()
    end
    
    -- Initialize local mouse to global (at frame start)
    state.mouseX = state.globalMouseX
    state.mouseY = state.globalMouseY

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

-- Get active widget ID
function state.getActive()
    return state.active
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
    local clicked = state.hot == id and state.mouseReleased and state.active == id
    if clicked then
        state.active = nil
    end
    return clicked
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

-- Parse hex color to RGB (0-1 range)
function state.hexToRgb(hex)
    if not hex or hex == "" then
        return 1, 1, 1
    end
    hex = hex:gsub("#", "")
    if #hex < 6 then return 1, 1, 1 end
    local r = tonumber(hex:sub(1, 2), 16) / 255
    local g = tonumber(hex:sub(3, 4), 16) / 255
    local b = tonumber(hex:sub(5, 6), 16) / 255
    return r or 1, g or 1, b or 1
end

return state
