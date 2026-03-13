--[[
  TextMixin
  Provides shared text labeling behavior for widgets (Label, Button, Checkbox, Slider, etc.).
]]

local state = require("ui.State")

local TextMixin = {}

-- Applies common text drawing given an LWC (Local Widget Context)
function TextMixin.draw(x, y, text, lwc)
    if not text or text == "" then return end
    
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local alpha = lwc:optNumber("opacity", 1.0)
    
    drawText(x, y, text, fgR, fgG, fgB, alpha)
end

function TextMixin.measure(text)
    if not text or text == "" then return 0 end
    return measureText(text)
end

function TextMixin.getLineHeight()
    return getLineHeight()
end

return TextMixin
