--[[
  Frequency Display Widget
  Modular UI component for radio frequency readout.
  Style and behavior driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local events = require("Events")

local FrequencyDisplay = {}
FrequencyDisplay.__index = FrequencyDisplay

-- Default rules for FrequencyDisplay widget (very low priority)
setbox.rule {
    id = "frequency-display-defaults",
    tags = {"widget.FrequencyDisplay"},
    priority = -1000,
    apply = {
        background = "#1e293b",
        foreground = "#ffffff",
        border = "#3b82f6",
        highlight = "#facc15", -- Yellow highlight for editing/focus
        highlighted = false,
        borderWidth = 1,
        borderRadius = 4,
        opacity = 1.0,
        height = 36,
    }
}

function FrequencyDisplay.new(props)
    local self = setmetatable({}, FrequencyDisplay)
    -- props can be nil for legacy callers, but should contain valueObs
    if props then
        self.valueObs = props.valueObs
    end
    return self
end


function formatFreq(f)
   local s = string.format("%.6fMHz", f)
   return s:sub(1, -7) .. "," .. s:sub(-6)
end


function FrequencyDisplay:draw(id, x, y, w, h, freqEntryText, cursorIdx, tags, parentLWC)
    local widgetTags = {"widget.FrequencyDisplay", "id." .. id}
    
    local isEditing = (freqEntryText and freqEntryText ~= "")
    local hasModeTag = events.hasModeTag("state.FreqEntryMode")
    
    if isEditing or hasModeTag then
        table.insert(widgetTags, "state.FreqEntryMode")
    end

    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end

    if state.isHot(id) then table.insert(widgetTags, "state.Hovered") end
    if state.isActive(id) then table.insert(widgetTags, "state.Active") end

    local lwc = setbox.newContext(widgetTags, parentLWC)
    
    -- Properties from rules
    w = w or lwc:getNumber("width")
    h = h or lwc:getNumber("height")
    
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, widgetTags)
    if state.pointInRect(state.mouseX, state.mouseY, x, y, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end

    local ui = require("ui.Widgets")
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = ui.hexToRgb(lwc:getString("foreground"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    
    if lwc:getBool("highlighted") then
        bR, bG, bB = ui.hexToRgb(lwc:getString("highlight"))
    end

    local bWidth = lwc:getNumber("borderWidth")
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")

    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end

    local frequency = 0
    if self.valueObs then
        frequency = self.valueObs:get()
    end
    
    local text = ""
    local isEditing = (freqEntryText and freqEntryText ~= "")
    if isEditing then
        text = freqEntryText
    else
        text = formatFreq(frequency / 1e6)
    end

    local tw = measureText(text)
    local tx = x + (w - tw) / 2
    local ty = y + (h - getLineHeight()) / 2
    drawText(tx, ty, text, fgR, fgG, fgB, alpha)

    -- Draw Cursor if editing
    if isEditing and cursorIdx then
        local blink = (_G.freqEntryBlink or 0) < 0.5
        if blink then
            local cursorOffset = measureText(text:sub(1, cursorIdx))
            local cursorX = tx + cursorOffset
            drawRect(cursorX, ty, 2, getLineHeight(), fgR, fgG, fgB, alpha)
        end
    end
end

return FrequencyDisplay
