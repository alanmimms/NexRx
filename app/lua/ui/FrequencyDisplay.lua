--[[
  Frequency Display Widget
  Modular UI component for radio frequency readout.
  Style and behavior driven entirely by SetBox rules.
]]

local setbox = require("setbox")

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
        borderWidth = 1,
        borderRadius = 4,
        opacity = 1.0,
        height = 36,
    }
}

function FrequencyDisplay.new()
    local self = setmetatable({}, FrequencyDisplay)
    return self
end

function FrequencyDisplay:draw(id, x, y, w, h, frequency, freqEntryText, tags, parentLWC)
    local widgetTags = {"widget.FrequencyDisplay", "id." .. id}
    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end
    
    local lwc = setbox.newContext(widgetTags, parentLWC)
    
    -- Properties from rules
    w = w or lwc:getNumber("width")
    h = h or lwc:getNumber("height")
    
    local bgR, bgG, bgB = require("ui.widgets").hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = require("ui.widgets").hexToRgb(lwc:getString("foreground"))
    local bR, bG, bB = require("ui.widgets").hexToRgb(lwc:getString("border"))
    local bWidth = lwc:getNumber("borderWidth")
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")

    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end

    local text = freqEntryText ~= "" and freqEntryText or string.format("%.3f MHz", frequency / 1e6)
    local tw = measureText(text)
    drawText(x + (w - tw) / 2, y + (h - getLineHeight()) / 2, text, fgR, fgG, fgB, alpha)
end

return FrequencyDisplay
