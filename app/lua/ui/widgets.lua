--[[
  UI Widgets Library
  Core NexRx UI components driven by SetBox rules.
]]

local state = require("ui.state")
local layout = require("ui.layout")

local ui = {}

-- Forward state/event functions for convenience
ui.beginFrame = function()
    state.beginFrame()
end
ui.endFrame = state.endFrame
ui.isHot = state.isHot
ui.isActive = state.isActive
ui.hasFocus = state.hasFocus
ui.wasClicked = state.wasClicked

function ui.setEventsModule(ev)
    state.setEventsModule(ev)
end

function ui.setLayoutModule(l)
    -- layout is already required locally, but we provide this for API compatibility
end

-- Parse hex color to RGB (0-1 range)
function ui.hexToRgb(hex)
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

-- Clamp helper
local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

-- Resolve widget size from rules or fallback
local function getWidgetSize(id, tags, defaultW, defaultH, parentLWC)
    local lwc = setbox.newContext(tags, parentLWC)
    local w = lwc:getNumber("width", defaultW)
    local h = lwc:getNumber("height", defaultH)
    return w, h
end

-- =============================================================================
-- Button Widget
-- =============================================================================
function ui.button(id, label, x, y, w, h, tags, parentLWC)
    local widgetTags = {"widget.Button"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end

    -- Add state tags
    if ui.isActive(id) then table.insert(widgetTags, "state.Pressed")
    elseif ui.isHot(id) then table.insert(widgetTags, "state.Hovered") end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    w, h = getWidgetSize(id, widgetTags, w, h, parentLWC)
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    -- Update state
    if state.pointInRect(state.mouseX, state.mouseY, x, y, w, h) then
        state.setHot(id)
        if state.mouseClicked then
            state.setActive(id)
        end
    end

    -- Resolve style from rules
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = ui.hexToRgb(lwc:getString("foreground"))
    local radius = lwc:getNumber("borderRadius")
    local bWidth = lwc:getNumber("borderWidth")
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))

    -- Draw
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, 1.0)
    drawRectOutline(x, y, w, h, bR, bG, bB, 1.0, bWidth)

    local labelW = measureText(label)
    local lineH = getLineHeight()
    drawText(x + (w - labelW) / 2, y + (h - lineH) / 2, label, fgR, fgG, fgB, 1.0)

    return state.wasClicked(id)
end

-- =============================================================================
-- Toggle Button
-- =============================================================================
function ui.toggle(id, label, x, y, w, h, checked, tags, parentLWC)
    local widgetTags = {"widget.Button", "widget.Toggle"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end
    if checked then table.insert(widgetTags, "state.Active") end
    
    ui.button(id, label, x, y, w, h, widgetTags, parentLWC)
end

-- =============================================================================
-- Checkbox Widget
-- =============================================================================
function ui.checkbox(id, label, x, y, checked, tags, property, parentLWC)
    local widgetTags = {"widget.Checkbox"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end
    if checked then table.insert(widgetTags, "state.Checked") end
    if ui.isActive(id) then table.insert(widgetTags, "state.Pressed")
    elseif ui.isHot(id) then table.insert(widgetTags, "state.Hovered") end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    local boxSize = lwc:getNumber("boxSize", 18)
    local spacing = lwc:getNumber("spacing", 8)
    local labelW = measureText(label)
    
    w, h = getWidgetSize(id, widgetTags, boxSize + spacing + labelW, boxSize, parentLWC)
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags, { property = property, checked = checked })

    -- Update state
    if state.pointInRect(state.mouseX, state.mouseY, x, y, w, h) then
        state.setHot(id)
        if state.mouseClicked then
            state.setActive(id)
        end
    end

    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = ui.hexToRgb(lwc:getString("foreground"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local aR, aG, aB = ui.hexToRgb(lwc:getString("accent"))
    local bWidth = lwc:getNumber("borderWidth")

    drawRoundedRect(x, y, boxSize, boxSize, 3, bgR, bgG, bgB, 1.0)
    drawRectOutline(x, y, boxSize, boxSize, bR, bG, bB, 1.0, bWidth)

    if checked then
        drawLine(x + 4, y + boxSize/2, x + boxSize/2 - 1, y + boxSize - 5, aR, aG, aB, 1.0, 2)
        drawLine(x + boxSize/2 - 1, y + boxSize - 5, x + boxSize - 4, y + 4, aR, aG, aB, 1.0, 2)
    end

    drawText(x + boxSize + spacing, y + (boxSize - getLineHeight()) / 2, label, fgR, fgG, fgB, 1.0)

    return state.wasClicked(id)
end

-- =============================================================================
-- Label Widget
-- =============================================================================
function ui.label(id, x, y, text, tags, parentLWC)
    local widgetTags = {"widget.Label"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    local fgR, fgG, fgB = ui.hexToRgb(lwc:getString("foreground"))
    
    local w, h = getWidgetSize(id, widgetTags, measureText(text), getLineHeight(), parentLWC)
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    drawText(x, y, text, fgR, fgG, fgB, 1.0)
end

-- =============================================================================
-- Panel Widget
-- =============================================================================
function ui.panel(id, x, y, w, h, tags, parentLWC)
    local widgetTags = {"widget.Panel"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border", "#00000000"))
    local bWidth = lwc:getNumber("borderWidth")
    local radius = lwc:getNumber("borderRadius", 0)

    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, 1.0)
    if bR then
        drawRectOutline(x, y, w, h, bR, bG, bB, 1.0, bWidth)
    end
end

-- =============================================================================
-- Slider Widget
-- =============================================================================
function ui.slider(id, x, y, w, minVal, maxVal, value, tags, property, parentLWC)
    local widgetTags = {"widget.Slider"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end
    if ui.isActive(id) then table.insert(widgetTags, "state.Active")
    elseif ui.isHot(id) then table.insert(widgetTags, "state.Hovered") end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    local h = lwc:getNumber("height", 8)
    local handleR = lwc:getNumber("handleRadius", 8)
    local bWidth = lwc:getNumber("borderWidth")
    
    state.registerWidget(id, {x=x - handleR, y=y - 4, w=w + handleR*2, h=h + 8}, widgetTags, {
        min = minVal, max = maxVal, value = value, property = property
    })

    -- Update state
    if state.pointInRect(state.mouseX, state.mouseY, x - handleR, y - 4, w + handleR*2, h + 8) then
        state.setHot(id)
        if state.mouseClicked then
            state.setActive(id)
        end
    end

    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local aR, aG, aB = ui.hexToRgb(lwc:getString("accent"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))

    local t = clamp((value - minVal) / (maxVal - minVal), 0, 1)
    
    -- Draw track
    drawRoundedRect(x, y, w, h, h/2, bgR, bgG, bgB, 1.0)
    -- Draw fill
    if t > 0 then
        drawRoundedRect(x, y, w * t, h, h/2, aR, aG, aB, 1.0)
    end
    -- Draw handle
    local hX = x + w * t
    local hY = y + h/2
    drawCircle(hX, hY, handleR, 1, 1, 1, 1)
    drawCircleOutline(hX, hY, handleR, bR, bG, bB, 1.0, bWidth)

    if ui.isActive(id) and state.mouseDown then
        local nt = clamp((state.mouseX - x) / w, 0, 1)
        return minVal + nt * (maxVal - minVal)
    end
    return value
end

-- =============================================================================
-- S-Meter Widget
-- =============================================================================
function ui.smeter(id, x, y, w, h, reading, tags, parentLWC)
    local widgetTags = {"widget.SMeter", "widget.Meter"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    w, h = getWidgetSize(id, widgetTags, w, 28, parentLWC)
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local weakR, weakG, weakB = ui.hexToRgb(lwc:getString("colorWeak"))
    local midR, midG, midB = ui.hexToRgb(lwc:getString("colorMid"))
    local strongR, strongG, strongB = ui.hexToRgb(lwc:getString("colorStrong"))
    local offR, offG, offB = ui.hexToRgb(lwc:getString("colorOff"))

    drawRoundedRect(x, y, w, h, 4, bgR, bgG, bgB, 1.0)

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

    local ty = y + h + 4
    local sW = measureText(reading.sText)
    local dW = measureText(reading.dBmText)
    local startX = x + (w - (sW + 10 + dW)) / 2
    drawText(startX, ty, reading.sText, 0.9, 0.9, 0.95, 1.0)
    drawText(startX + sW + 10, ty, reading.dBmText, 0.5, 0.5, 0.55, 1.0)
end

-- =============================================================================
-- Frequency Display Widget
-- =============================================================================
function ui.frequencyDisplay(id, x, y, w, h, frequency, freqEntryText, tags, parentLWC)
    local widgetTags = {"widget.FrequencyDisplay", "widget.VFOControl"}
    if tags then
        for _, t in ipairs(tags) do
            table.insert(widgetTags, t:find("%.") and t or ("widget." .. t))
        end
    end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    w, h = getWidgetSize(id, widgetTags, w, 36, parentLWC)
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)

    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = ui.hexToRgb(lwc:getString("foreground"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local bWidth = lwc:getNumber("borderWidth")

    drawRoundedRect(x, y, w, h, 4, bgR, bgG, bgB, 1.0)
    drawRectOutline(x, y, w, h, bR, bG, bB, 1.0, bWidth)

    local text = freqEntryText ~= "" and freqEntryText or string.format("%.3f MHz", frequency / 1e6)
    local tw = measureText(text)
    drawText(x + (w - tw) / 2, y + (h - getLineHeight()) / 2, text, fgR, fgG, fgB, 1.0)
end

-- Legacy graticule helper
function ui.graticuleLegend(id, x, y, w, h, hText, vText)
    drawText(x, y, hText, 0.6, 0.6, 0.7, 0.8)
    drawText(x, y + 18, vText, 0.6, 0.6, 0.7, 0.8)
end

-- Active Tags Viewer (Debug)
function ui.activeTagsViewer(id, x, y, w, h, tags)
    drawRoundedRect(x, y, w, h, 4, 0.05, 0.05, 0.1, 0.8)
    drawText(x + 8, y + 8, "ACTIVE TAGS", 0.5, 0.7, 1.0, 1.0)
    local ty = y + 32
    for _, tag in ipairs(tags) do
        drawText(x + 12, ty, tag, 0.8, 0.8, 0.8, 1.0)
        ty = ty + 18
        if ty > y + h - 20 then break end
    end
end

return ui
