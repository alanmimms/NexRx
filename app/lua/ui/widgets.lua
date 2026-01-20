--[[
  Event-Driven Widget System

  Widgets register themselves with the events module for SetBox-based
  event dispatch. Widgets are purely presentational - they draw themselves
  based on passed state but don't handle clicks directly.

  Click/interaction handling flows through:
  1. Widget registers with events module (with tags)
  2. Mouse click dispatched through events.dispatch()
  3. SetBox resolves handler based on widget tags
  4. Handler performs action and updates state

  Usage:
    local ui = require("ui.widgets")
    local events = require("events")
    ui.setEventsModule(events)

    -- Register handler
    events.registerHandler("my_button_click", function(event, widget)
        print("Button clicked!")
        return true
    end)

    function draw()
        ui.beginFrame()
        -- Button draws itself, click handled via event dispatch
        ui.button("btn1", "Click Me", 50, 50, 100, 32, {"MyButton"})
        ui.endFrame()
    end
]]

local state = require("ui.state")
local theme = require("ui.theme")

local ui = {}

-- Events module reference (required for event dispatch)
local eventsModule = nil

-- Layout module reference (for getting current region ID)
local layoutModule = nil

-- Set events module for widget registration
function ui.setEventsModule(events)
    eventsModule = events
end

-- Set layout module for parent hierarchy
function ui.setLayoutModule(layout)
    layoutModule = layout
end

-- Internal: register widget with events module
local function registerWidget(id, bounds, widgetTags)
    if eventsModule and eventsModule.registerWidget then
        local parentId = nil
        if layoutModule and layoutModule.getCurrentRegionId then
            parentId = layoutModule.getCurrentRegionId()
        end
        eventsModule.registerWidget(id, bounds, widgetTags, parentId)
    end
end

-- Forward state functions
ui.beginFrame = function()
    state.beginFrame()
    theme.beginFrame()
end
ui.endFrame = state.endFrame
ui.isHot = state.isHot
ui.isActive = state.isActive
ui.hasFocus = state.hasFocus

-- Clamp helper
local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

-- =============================================================================
-- Button Widget
-- Purely presentational - click handling via event dispatch
-- =============================================================================
function ui.button(id, label, x, y, w, h, tags)
    w = w or 100
    h = h or 32

    -- Build widget tags
    local widgetTags = {"Button"}
    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end

    -- Register with events module for SetBox dispatch
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    -- Track hot/active state for visual feedback only
    local isHot = state.pointInRect(state.mouseX, state.mouseY, x, y, w, h)
    if isHot then
        state.setHot(id)
        if state.mouseDown then
            state.setActive(id)
        end
    end

    -- Get style from theme
    local style = theme.getButtonStyle(tags, state.isHot(id), state.isActive(id), false)

    -- Draw button background
    drawRoundedRect(x, y, w, h, style.borderRadius, style.bgR, style.bgG, style.bgB, 1.0)

    -- Draw border if active/hot
    if state.isHot(id) or state.isActive(id) then
        drawRectOutline(x, y, w, h, style.accentR, style.accentG, style.accentB, 1.0, style.borderWidth)
    end

    -- Draw label centered
    local labelW = measureText(label)
    local lineH = getLineHeight()
    local textX = x + (w - labelW) / 2
    local textY = y + (h - lineH) / 2
    drawText(textX, textY, label, style.fgR, style.fgG, style.fgB, 1.0)

    -- No return value - click handling via events
end

-- =============================================================================
-- Toggle Widget
-- Like button but displays checked state visually
-- =============================================================================
function ui.toggle(id, label, x, y, w, h, checked, tags)
    w = w or 100
    h = h or 32

    -- Build widget tags
    local widgetTags = {"Toggle", "Button"}
    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end
    if checked then
        table.insert(widgetTags, "Checked")
    end

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    -- Track hot/active for visual feedback
    local isHot = state.pointInRect(state.mouseX, state.mouseY, x, y, w, h)
    if isHot then
        state.setHot(id)
        if state.mouseDown then
            state.setActive(id)
        end
    end

    -- Modify display tags based on state
    local displayTags = tags and {table.unpack(tags)} or {}
    if checked then
        table.insert(displayTags, "Active")
    end

    local style = theme.getButtonStyle(displayTags, state.isHot(id), state.isActive(id), false)

    -- Draw button
    drawRoundedRect(x, y, w, h, style.borderRadius, style.bgR, style.bgG, style.bgB, 1.0)

    if state.isHot(id) or state.isActive(id) or checked then
        drawRectOutline(x, y, w, h, style.accentR, style.accentG, style.accentB, 1.0, style.borderWidth)
    end

    local labelW = measureText(label)
    local lineH = getLineHeight()
    drawText(x + (w - labelW) / 2, y + (h - lineH) / 2, label, style.fgR, style.fgG, style.fgB, 1.0)

    -- No return value - state changes via event handlers
end

-- =============================================================================
-- Checkbox Widget
-- =============================================================================
function ui.checkbox(id, label, x, y, checked, tags)
    local boxSize = 18
    local spacing = 8
    local labelW = measureText(label)
    local totalW = boxSize + spacing + labelW
    local h = boxSize

    -- Build widget tags
    local widgetTags = {"Checkbox"}
    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end
    if checked then
        table.insert(widgetTags, "Checked")
    end

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=totalW, h=h}, widgetTags)

    -- Track hot/active for visual feedback
    local isHot = state.pointInRect(state.mouseX, state.mouseY, x, y, totalW, h)
    if isHot then
        state.setHot(id)
        if state.mouseDown then
            state.setActive(id)
        end
    end

    local style = theme.getCheckboxStyle(tags, state.isHot(id), state.isActive(id), checked)

    -- Draw checkbox box
    drawRoundedRect(x, y, boxSize, boxSize, 3, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, boxSize, boxSize, style.borderR, style.borderG, style.borderB, 1.0, 1)

    -- Draw checkmark if checked
    if checked then
        drawLine(x + 4, y + boxSize/2, x + boxSize/2 - 1, y + boxSize - 5,
                 style.accentR, style.accentG, style.accentB, 1.0, 2)
        drawLine(x + boxSize/2 - 1, y + boxSize - 5, x + boxSize - 4, y + 4,
                 style.accentR, style.accentG, style.accentB, 1.0, 2)
    end

    -- Draw label
    local lineH = getLineHeight()
    local textY = y + (boxSize - lineH) / 2
    drawText(x + boxSize + spacing, textY, label, style.fgR, style.fgG, style.fgB, 1.0)

    -- No return value - state changes via event handlers
end

-- =============================================================================
-- Slider Widget
-- Handles dragging internally for smooth UX, dispatches value via callback
-- =============================================================================
function ui.slider(id, x, y, w, minVal, maxVal, value, tags)
    local h = 8
    local handleR = 10
    local hitH = math.max(h, handleR * 2)
    local hitY = y - (hitH - h) / 2

    -- Build widget tags
    local widgetTags = {"Slider"}
    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end

    -- Register with events module
    registerWidget(id, {x=x - handleR, y=hitY, w=w + handleR*2, h=hitH}, widgetTags)

    -- Normalize value
    local t = (value - minVal) / (maxVal - minVal)
    t = clamp(t, 0, 1)

    local isHot = state.pointInRect(state.mouseX, state.mouseY, x - handleR, hitY, w + handleR*2, hitH)
    if isHot then
        state.setHot(id)
        if state.mouseClicked then
            state.setActive(id)
        end
    end

    -- Handle dragging - sliders still track this for smooth visual feedback
    -- The caller must check the return value and update state
    local newValue = value
    if state.isActive(id) and state.mouseDown then
        t = clamp((state.mouseX - x) / w, 0, 1)
        newValue = minVal + t * (maxVal - minVal)
    end

    local style = theme.getSliderStyle(tags, state.isHot(id), state.isActive(id))

    -- Draw track
    drawRoundedRect(x, y, w, h, h/2, style.bgR, style.bgG, style.bgB, 1.0)

    -- Draw filled portion (use newValue for immediate visual feedback)
    local displayT = (newValue - minVal) / (maxVal - minVal)
    displayT = clamp(displayT, 0, 1)
    local fillW = w * displayT
    if fillW > 0 then
        drawRoundedRect(x, y, fillW, h, h/2, style.accentR, style.accentG, style.accentB, 1.0)
    end

    -- Draw handle
    local handleX = x + fillW
    local handleY = y + h/2
    drawCircle(handleX, handleY, handleR, 1.0, 1.0, 1.0, 1.0)
    drawCircleOutline(handleX, handleY, handleR, style.borderR, style.borderG, style.borderB, 1.0, 2)

    -- Sliders return new value for smooth drag feedback
    -- Caller should update state if value changed
    return newValue
end

-- =============================================================================
-- Vertical Slider Widget
-- =============================================================================
function ui.vslider(id, x, y, h, minVal, maxVal, value, tags)
    local w = 8
    local handleR = 10
    local hitW = math.max(w, handleR * 2)
    local hitX = x - (hitW - w) / 2

    -- Build widget tags
    local widgetTags = {"Slider", "Vertical"}
    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end

    -- Register with events module
    registerWidget(id, {x=hitX, y=y - handleR, w=hitW, h=h + handleR*2}, widgetTags)

    -- Normalize value (top = max, bottom = min)
    local t = (value - minVal) / (maxVal - minVal)
    t = clamp(t, 0, 1)

    local isHot = state.pointInRect(state.mouseX, state.mouseY, hitX, y - handleR, hitW, h + handleR*2)
    if isHot then
        state.setHot(id)
        if state.mouseClicked then
            state.setActive(id)
        end
    end

    -- Handle dragging
    local newValue = value
    if state.isActive(id) and state.mouseDown then
        t = 1.0 - clamp((state.mouseY - y) / h, 0, 1)
        newValue = minVal + t * (maxVal - minVal)
    end

    local style = theme.getSliderStyle(tags, state.isHot(id), state.isActive(id))

    -- Draw track
    drawRoundedRect(x, y, w, h, w/2, style.bgR, style.bgG, style.bgB, 1.0)

    -- Draw filled portion (from bottom)
    local displayT = (newValue - minVal) / (maxVal - minVal)
    displayT = clamp(displayT, 0, 1)
    local fillH = h * displayT
    if fillH > 0 then
        drawRoundedRect(x, y + h - fillH, w, fillH, w/2, style.accentR, style.accentG, style.accentB, 1.0)
    end

    -- Draw handle
    local handleX = x + w/2
    local handleY = y + h - fillH
    drawCircle(handleX, handleY, handleR, 1.0, 1.0, 1.0, 1.0)
    drawCircleOutline(handleX, handleY, handleR, style.borderR, style.borderG, style.borderB, 1.0, 2)

    return newValue
end

-- =============================================================================
-- Non-Interactive Widgets
-- =============================================================================

-- Progress bar (non-interactive)
function ui.progressBar(x, y, w, h, value, tags)
    local t = clamp(value, 0, 1)

    local style = theme.getStyle(tags or {"ProgressBar"})

    -- Draw track
    drawRoundedRect(x, y, w, h, h/2, style.bgR, style.bgG, style.bgB, 1.0)

    -- Draw fill
    local fillW = w * t
    if fillW > 0 then
        drawRoundedRect(x, y, fillW, h, h/2, style.accentR, style.accentG, style.accentB, 1.0)
    end
end

-- Label (non-interactive text)
function ui.label(x, y, text, tags)
    local style = theme.getLabelStyle(tags)
    drawText(x, y, text, style.fgR, style.fgG, style.fgB, 1.0)
end

-- Panel background
function ui.panel(x, y, w, h, tags)
    local style = theme.getPanelStyle(tags)
    drawRoundedRect(x, y, w, h, style.borderRadius, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, style.borderWidth)
end

-- Panel with title
function ui.panelWithTitle(x, y, w, h, title, tags)
    local style = theme.getPanelStyle(tags)
    local titleH = 28

    -- Panel body
    drawRoundedRect(x, y, w, h, style.borderRadius, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, style.borderWidth)

    -- Title bar (slightly different shade)
    drawRoundedRect(x + 1, y + 1, w - 2, titleH, style.borderRadius - 1,
                    style.bgR * 0.8, style.bgG * 0.8, style.bgB * 0.8, 1.0)
    drawLine(x, y + titleH, x + w, y + titleH, style.borderR, style.borderG, style.borderB, 1.0, 1)

    -- Title text
    local labelStyle = theme.getLabelStyle({"Title"})
    local lineH = getLineHeight()
    drawText(x + style.padding, y + (titleH - lineH) / 2, title, labelStyle.fgR, labelStyle.fgG, labelStyle.fgB, 1.0)

    -- Return content area
    return x + style.padding, y + titleH + style.padding, w - style.padding * 2, h - titleH - style.padding * 2
end

-- Separator line
function ui.separator(x, y, w, tags)
    local style = theme.getStyle(tags or {"Separator"})
    drawLine(x, y, x + w, y, style.borderR, style.borderG, style.borderB, 0.5, 1)
end

-- =============================================================================
-- Text Input Widget
-- =============================================================================
function ui.textInput(id, x, y, w, text, placeholder, tags)
    local h = 28
    local padding = 8

    -- Build widget tags
    local widgetTags = {"Input", "TextInput"}
    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local isHot = state.pointInRect(state.mouseX, state.mouseY, x, y, w, h)
    if isHot then
        state.setHot(id)
        if state.mouseClicked then
            state.setActive(id)
            state.setFocus(id)
        end
    end

    -- Click outside clears focus
    if state.mouseClicked and not isHot and state.hasFocus(id) then
        state.clearFocus()
    end

    local style = theme.getInputStyle(tags, state.isHot(id), state.hasFocus(id))

    -- Draw background
    drawRoundedRect(x, y, w, h, style.borderRadius, style.bgR, style.bgG, style.bgB, 1.0)

    -- Draw border (accent if focused)
    if state.hasFocus(id) then
        drawRectOutline(x, y, w, h, style.accentR, style.accentG, style.accentB, 1.0, 2)
    else
        drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, 1)
    end

    -- Draw text or placeholder
    local lineH = getLineHeight()
    local textY = y + (h - lineH) / 2
    if text and #text > 0 then
        drawText(x + padding, textY, text, style.fgR, style.fgG, style.fgB, 1.0)
    elseif placeholder then
        drawText(x + padding, textY, placeholder, style.fgR * 0.5, style.fgG * 0.5, style.fgB * 0.5, 1.0)
    end

    -- Draw cursor if focused
    if state.hasFocus(id) then
        local textW = measureText(text or "")
        local cursorX = x + padding + textW
        drawLine(cursorX, y + 6, cursorX, y + h - 6, style.fgR, style.fgG, style.fgB, 1.0, 1)
    end

    return text
end

return ui
