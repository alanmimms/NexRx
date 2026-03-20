--[[
  TextMixin
  Provides shared text labeling behavior for widgets (Label, Button, Checkbox, Slider, etc.).
]]

local state = require("ui.State")

local TextMixin = {}

-- Applies common text drawing given an LWC (Local Widget Context)
function TextMixin.draw(x, y, text, lwc, fontSize)
    if not text or text == "" then return end
    
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local alpha = lwc:optNumber("opacity", 1.0)
    local fs = fontSize or lwc:optNumber("fontSize", 20)
    
    -- Localized draw via System
    System.drawText(tostring(text), x, y, fs, {fgR, fgG, fgB, alpha})
end

function TextMixin.measure(text, fontSize)
    if not text or text == "" then return 0 end
    return System.measureText(tostring(text), fontSize or 20)
end

function TextMixin.getLineHeight()
    -- Use static default or query from System
    return 20
end

return TextMixin
