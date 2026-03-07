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
local layoutOverrides = require("layout_overrides")
local constraints = require("ui.constraints")

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

-- Internal: Get widget dimensions from the proper hierarchy:
-- 1. layoutOverrides (user dragged)
-- 2. constraints/rules (SetBox)
-- 3. Minimal safety fallback (should never be needed if rules are complete)
local function getWidgetSize(id, widgetTags, passedW, passedH)
    -- First priority: user overrides from dragging
    local w = layoutOverrides.get(id, "width")
    local h = layoutOverrides.get(id, "height")

    -- Second priority: passed values (caller may have computed from context)
    if not w then w = passedW end
    if not h then h = passedH end

    -- Third priority: SetBox rules
    if not w or not h then
        local cons = constraints.query(widgetTags)
        if not w and cons.width then
            w = constraints.eval(cons.width, {})
        end
        if not h and cons.height then
            h = constraints.eval(cons.height, {})
        end
    end

    -- Last resort fallback (rules should cover this, but safety first)
    w = w or 80
    h = h or 24

    return w, h
end

-- Internal: register widget with events module
-- @param data optional widget-specific data (e.g., {min=0, max=100, property="volume"})
local function registerWidget(id, bounds, widgetTags, data)
    if eventsModule and eventsModule.registerWidget then
        local parentId = nil
        if layoutModule and layoutModule.getCurrentRegionId then
            parentId = layoutModule.getCurrentRegionId()
        end
        eventsModule.registerWidget(id, bounds, widgetTags, parentId, data)
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
    -- Build widget tags first (needed for size lookup)
    local widgetTags = {"widget.Button"}
    if tags then
        for _, t in ipairs(tags) do
            -- Add widget. prefix if not already namespaced
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end

    -- Get size: overrides → passed values → rules → fallback
    w, h = getWidgetSize(id, widgetTags, w, h)

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
    -- Build widget tags first (needed for size lookup)
    local widgetTags = {"widget.Toggle", "widget.Button"}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end
    if checked then
        table.insert(widgetTags, "state.Checked")
    end

    -- Get size: overrides → passed values → rules → fallback
    w, h = getWidgetSize(id, widgetTags, w, h)

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

    -- Modify display tags based on state (namespaced)
    local displayTags = {}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(displayTags, "widget." .. t)
            else
                table.insert(displayTags, t)
            end
        end
    end
    if checked then
        table.insert(displayTags, "state.Active")
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
function ui.checkbox(id, label, x, y, checked, tags, property)
    -- Build widget tags first
    local widgetTags = {"widget.Checkbox"}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end
    if checked then
        table.insert(widgetTags, "state.Checked")
    end

    -- Compute content-based defaults
    local boxSize = 18
    local spacing = 8
    local labelW = measureText(label)
    local defaultW = boxSize + spacing + labelW
    local defaultH = boxSize

    -- Get size: overrides → content-based defaults → rules
    local w, h = getWidgetSize(id, widgetTags, defaultW, defaultH)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags, {
        property = property,
        checked = checked
    })

    -- Track hot/active for visual feedback
    local isHot = state.pointInRect(state.mouseX, state.mouseY, x, y, w, h)
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
-- Slider widget
-- @param id unique widget ID
-- @param x, y, w position and width
-- @param minVal, maxVal value range
-- @param value current value
-- @param tags optional tags array
-- @param property optional property name for event-based adjustment
function ui.slider(id, x, y, w, minVal, maxVal, value, tags, property)
    -- Build widget tags first
    local widgetTags = {"widget.Slider"}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    local h
    w, h = getWidgetSize(id, widgetTags, w, 8)

    local handleR = 10
    local hitH = math.max(h, handleR * 2)
    local hitY = y - (hitH - h) / 2

    -- Register with events module, including slider data for generic handlers
    registerWidget(id, {x=x - handleR, y=hitY, w=w + handleR*2, h=hitH}, widgetTags, {
        min = minVal,
        max = maxVal,
        value = value,
        property = property,  -- Optional: property name for event-based adjustment
    })

    -- Normalize value
    local t = (value - minVal) / (maxVal - minVal)
    t = clamp(t, 0, 1)

    local isHot = state.pointInRect(state.mouseX, state.mouseY, x - handleR, hitY, w + handleR*2, hitH)
    if isHot then
        state.setHot(id)
        -- Activation is handled by slider_activate rule, not direct click
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
    -- Build widget tags first
    local widgetTags = {"widget.Slider", "widget.Vertical"}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    local w
    w, h = getWidgetSize(id, widgetTags, 8, h)

    local handleR = 10
    local hitW = math.max(w, handleR * 2)
    local hitX = x - (hitW - w) / 2

    -- Register with events module
    registerWidget(id, {x=hitX, y=y - handleR, w=hitW, h=h + handleR*2}, widgetTags)

    -- Normalize value (top = max, bottom = min)
    local t = (value - minVal) / (maxVal - minVal)
    t = clamp(t, 0, 1)

    local isHot = state.pointInRect(state.mouseX, state.mouseY, hitX, y - handleR, hitW, h + handleR*2)
    if isHot then
        state.setHot(id)
        -- Activation is handled by slider_activate rule, not direct click
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
-- Display Widgets (may or may not have event handlers - rules decide)
-- =============================================================================

-- Progress bar
function ui.progressBar(id, x, y, w, h, value, tags)
    -- Build widget tags first
    local widgetTags = {"widget.ProgressBar"}
    if tags then
        for _, tag in ipairs(tags) do
            if not tag:find("%.") then
                table.insert(widgetTags, "widget." .. tag)
            else
                table.insert(widgetTags, tag)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    w, h = getWidgetSize(id, widgetTags, w, h)

    local t = clamp(value, 0, 1)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local style = theme.getStyle(widgetTags)

    -- Draw track
    drawRoundedRect(x, y, w, h, h/2, style.bgR, style.bgG, style.bgB, 1.0)

    -- Draw fill
    local fillW = w * t
    if fillW > 0 then
        drawRoundedRect(x, y, fillW, h, h/2, style.accentR, style.accentG, style.accentB, 1.0)
    end
end

-- Label (text display)
function ui.label(id, x, y, text, tags)
    -- Build widget tags first
    local widgetTags = {"widget.Label"}
    if tags then
        for _, tag in ipairs(tags) do
            if not tag:find("%.") then
                table.insert(widgetTags, "widget." .. tag)
            else
                table.insert(widgetTags, tag)
            end
        end
    end

    -- Content-based defaults
    local defaultW = measureText(text)
    local defaultH = getLineHeight()

    -- Get size: overrides → content-based → rules
    local w, h = getWidgetSize(id, widgetTags, defaultW, defaultH)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local style = theme.getLabelStyle(tags)
    drawText(x, y, text, style.fgR, style.fgG, style.fgB, 1.0)
end

-- Panel background
function ui.panel(id, x, y, w, h, tags)
    -- Build widget tags first
    local widgetTags = {"widget.Panel"}
    if tags then
        for _, tag in ipairs(tags) do
            if not tag:find("%.") then
                table.insert(widgetTags, "widget." .. tag)
            else
                table.insert(widgetTags, tag)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    w, h = getWidgetSize(id, widgetTags, w, h)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local style = theme.getPanelStyle(tags)
    drawRoundedRect(x, y, w, h, style.borderRadius, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, style.borderWidth)
end

-- Panel with title
function ui.panelWithTitle(id, x, y, w, h, title, tags)
    -- Build widget tags first
    local widgetTags = {"widget.Panel", "widget.TitledPanel"}
    if tags then
        for _, tag in ipairs(tags) do
            if not tag:find("%.") then
                table.insert(widgetTags, "widget." .. tag)
            else
                table.insert(widgetTags, tag)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    w, h = getWidgetSize(id, widgetTags, w, h)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

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
    local labelStyle = theme.getLabelStyle({"widget.Title"})
    local lineH = getLineHeight()
    drawText(x + style.padding, y + (titleH - lineH) / 2, title, labelStyle.fgR, labelStyle.fgG, labelStyle.fgB, 1.0)

    -- Return content area
    return x + style.padding, y + titleH + style.padding, w - style.padding * 2, h - titleH - style.padding * 2
end

-- Separator line
function ui.separator(id, x, y, w, tags)
    -- Build widget tags first
    local widgetTags = {"widget.Separator"}
    if tags then
        for _, tag in ipairs(tags) do
            if not tag:find("%.") then
                table.insert(widgetTags, "widget." .. tag)
            else
                table.insert(widgetTags, tag)
            end
        end
    end

    -- Get size: overrides → passed values → rules (default height = 1 for separators)
    local h
    w, h = getWidgetSize(id, widgetTags, w, 1)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local style = theme.getStyle(widgetTags)
    drawLine(x, y, x + w, y, style.borderR, style.borderG, style.borderB, 0.5, 1)
end

-- Meter widget (like S-meter) - a bar graph display
function ui.meter(id, x, y, w, h, value, minVal, maxVal, tags)
    -- Build widget tags first
    local widgetTags = {"widget.Meter"}
    if tags then
        for _, tag in ipairs(tags) do
            if not tag:find("%.") then
                table.insert(widgetTags, "widget." .. tag)
            else
                table.insert(widgetTags, tag)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    w, h = getWidgetSize(id, widgetTags, w, h)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local style = theme.getStyle(widgetTags)

    -- Draw background
    drawRoundedRect(x, y, w, h, 4, style.bgR or 0.12, style.bgG or 0.12, style.bgB or 0.15, 1.0)

    -- Draw filled portion
    local t = clamp((value - minVal) / (maxVal - minVal), 0, 1)
    local fillW = (w - 8) * t
    if fillW > 0 then
        drawRect(x + 4, y + 4, fillW, h - 8, style.accentR or 0.2, style.accentG or 0.8, style.accentB or 0.3, 1.0)
    end
end

-- =============================================================================
-- S-Meter Widget
-- =============================================================================
function ui.smeter(id, x, y, w, h, reading, tags)
    -- Build widget tags first
    local widgetTags = {"widget.SMeter", "widget.Meter"}
    if tags then
        for _, tag in ipairs(tags) do
            if not tag:find("%.") then
                table.insert(widgetTags, "widget." .. tag)
            else
                table.insert(widgetTags, tag)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    w, h = getWidgetSize(id, widgetTags, w, 28)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    -- Get colors from SetBox
    local prevTags = setbox.getActiveTags()
    setbox.setActiveTags(widgetTags)
    local bgR, bgG, bgB = theme.hexToRgb(setbox.getString("background", "#1e293b"))
    local weakR, weakG, weakB = theme.hexToRgb(setbox.getString("colorWeak", "#22c55e"))
    local midR, midG, midB = theme.hexToRgb(setbox.getString("colorMid", "#eab308"))
    local strongR, strongG, strongB = theme.hexToRgb(setbox.getString("colorStrong", "#ef4444"))
    local offR, offG, offB = theme.hexToRgb(setbox.getString("colorOff", "#334155"))
    setbox.setActiveTags(prevTags)

    -- Draw background
    drawRoundedRect(x, y, w, h, 4, bgR, bgG, bgB, 1.0)

    -- Draw segments
    local barCount = 12
    local barPad = 4
    local barGap = 2
    local barW = (w - barPad * 2 - barGap * (barCount - 1)) / barCount
    local barH = h - barPad * 2

    for i = 0, barCount - 1 do
        local barX = x + barPad + i * (barW + barGap)
        local barThreshold = (i < 9) and (i + 1) or (9 + (i - 8) * (10/6))

        if reading.sUnits >= barThreshold then
            local r, g, b = weakR, weakG, weakB
            if i >= 9 then r, g, b = strongR, strongG, strongB
            elseif i >= 6 then r, g, b = midR, midG, midB end
            drawRect(barX, y + barPad, barW, barH, r, g, b, 1.0)
        else
            drawRect(barX, y + barPad, barW, barH, offR, offG, offB, 1.0)
        end
    end

    -- Draw text info below
    local ty = y + h + 4
    local sTextW = measureText(reading.sText)
    local dBmTextW = measureText(reading.dBmText)
    local totalTextW = sTextW + 10 + dBmTextW
    local textStartX = x + (w - totalTextW) / 2

    drawText(textStartX, ty, reading.sText, 0.9, 0.9, 0.95, 1.0)
    drawText(textStartX + sTextW + 10, ty, reading.dBmText, 0.5, 0.5, 0.55, 1.0)

    return h + 24 -- Approximate height consumed
end

-- =============================================================================
-- Frequency Display Widget
-- =============================================================================
function ui.frequencyDisplay(id, x, y, w, h, frequency, freqEntryText, tags)
    -- Build widget tags first
    local widgetTags = {"widget.FrequencyDisplay", "widget.VFOControl"}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    w, h = getWidgetSize(id, widgetTags, w, 36)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    -- Style from theme
    local style = theme.getStyle(widgetTags)

    -- Draw background
    drawRoundedRect(x, y, w, h, 4, 0.1, 0.1, 0.15, 1.0)

    -- Draw frequency text (prefer entry text if available)
    local displayStr = freqEntryText and freqEntryText ~= "" and freqEntryText or string.format("%.6f", frequency)
    local textW = measureText(displayStr)
    local lineH = getLineHeight()
    local textX = x + (w - textW) / 2
    local textY = y + (h - lineH) / 2

    -- Greenish color for frequency
    local r, g, b = 0.2, 0.9, 0.4
    if freqEntryText and freqEntryText ~= "" then
        r, g, b = 0.9, 0.9, 0.4 -- Yellow for entry mode
    end

    drawText(textX, textY, displayStr, r, g, b, 1.0)

    -- Show cursor if in entry mode
    if freqEntryText and freqEntryText ~= "" then
        local cursorX = textX + textW
        drawLine(cursorX, y + 8, cursorX, y + h - 8, r, g, b, 1.0, 1)
    end
end

-- =============================================================================
-- Text Input Widget
-- =============================================================================
function ui.textInput(id, x, y, w, text, placeholder, tags)
    -- Build widget tags first
    local widgetTags = {"widget.Input", "widget.TextInput"}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    local h
    w, h = getWidgetSize(id, widgetTags, w, 28)

    local padding = 8

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

-- =============================================================================
-- Active Tags Viewer Widget (Debug)
-- Displays all currently active tags in real-time
-- =============================================================================
function ui.activeTagsViewer(id, x, y, w, h, activeTags, tags)
    -- Build widget tags first
    local widgetTags = {"widget.ActiveTagsViewer", "widget.DebugWidget", "widget.Panel"}
    if tags then
        for _, t in ipairs(tags) do
            if not t:find("%.") then
                table.insert(widgetTags, "widget." .. t)
            else
                table.insert(widgetTags, t)
            end
        end
    end

    -- Get size: overrides → passed values → rules
    w, h = getWidgetSize(id, widgetTags, w, h)

    -- Register with events module
    registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local lineH = getLineHeight()
    local padding = 8

    -- Draw panel background
    drawRoundedRect(x, y, w, h, 4, 0.08, 0.08, 0.12, 1.0)
    drawRectOutline(x, y, w, h, 0.2, 0.2, 0.25, 1.0, 1.0)

    -- Draw title
    drawText(x + padding, y + padding, "Active Tags", 0.6, 0.7, 0.9, 1.0)

    -- Sort and display active tags
    local sortedTags = {}
    if activeTags then
        for tagName, _ in pairs(activeTags) do
            table.insert(sortedTags, tagName)
        end
        table.sort(sortedTags)
    end

    -- Draw each tag
    local textY = y + padding + lineH + 8
    local maxLines = math.floor((h - padding * 2 - lineH - 8) / (lineH + 2))

    for i, tagName in ipairs(sortedTags) do
        if i > maxLines then
            -- Show overflow indicator
            drawText(x + padding, textY, "..." .. (#sortedTags - maxLines + 1) .. " more", 0.5, 0.5, 0.55, 1.0)
            break
        end

        -- Color code by namespace prefix
        local r, g, b = 0.7, 0.7, 0.75  -- Default gray
        if tagName:match("^event%.") then
            r, g, b = 1.0, 0.4, 0.4  -- Red for transient events
        elseif tagName:match("^widget%.") then
            r, g, b = 0.4, 0.9, 0.4  -- Green for widget tags
        elseif tagName:match("^state%.") then
            r, g, b = 0.9, 0.4, 0.9  -- Magenta for state tags
        elseif tagName:match("^input%.") then
            r, g, b = 0.2, 0.8, 0.9  -- Cyan for input/held tags
        elseif tagName:match("^layout%.") then
            r, g, b = 0.9, 0.7, 0.3  -- Orange for layout tags
        end

        drawText(x + padding, textY, tagName, r, g, b, 1.0)
        textY = textY + lineH + 2
    end

    -- Show tag count at bottom
    local countText = string.format("%d tags", #sortedTags)
    local countW = measureText(countText)
    drawText(x + w - padding - countW, y + h - padding - lineH, countText, 0.5, 0.5, 0.55, 1.0)
end

--- Spectrum Graticule Legend
-- @param id Unique widget ID
-- @param x, y position
-- @param w, h size
-- @param xUnits horizontal scale string (e.g. "10 kHz/div")
-- @param yUnits vertical scale string (e.g. "10 dB/div")
-- @param tags optional tags
function ui.graticuleLegend(id, x, y, w, h, xUnits, yUnits, tags)
    local widgetTags = {"widget.GraticuleLegend"}
    if tags then for _, t in ipairs(tags) do table.insert(widgetTags, t) end end
    
    w, h = getWidgetSize(id, widgetTags, w, h)
    local bounds = {x=x, y=y, w=w, h=h}
    registerWidget(id, bounds, widgetTags)
    
    local style = theme.getStyle(widgetTags)
    local bg = style.bgR and {style.bgR, style.bgG, style.bgB, 0.7} or {0.05, 0.05, 0.05, 0.7}
    local fg = style.fgR and {style.fgR, style.fgG, style.fgB, 1.0} or {0.7, 0.7, 0.7, 1.0}
    
    drawRect(x, y, w, h, bg[1], bg[2], bg[3], bg[4])
    drawText(x + 6, y + 6, xUnits, fg[1], fg[2], fg[3], fg[4])
    drawText(x + 6, y + 22, yUnits, fg[1], fg[2], fg[3], fg[4])
end

return ui
