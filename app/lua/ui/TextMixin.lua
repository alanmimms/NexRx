--[[
  TextMixin
  Provides shared text labeling behavior for widgets (Label, Button, Checkbox, Slider, etc.).
]]

local state = require("ui.State")

local TextMixin = {}

-- Applies common text drawing given a widget or LWC (Local Widget Context)
function TextMixin.draw(x, y, text, target, fontSize)
    if not text or text == "" then return end

    local props = target and target.props or target
    local fgStr = "#ffffff"
    local alpha = 1.0
    local fs = fontSize

    if props and type(props) == "table" and not props.optString then
        fgStr = props.foreground or props.textColor or "#ffffff"
        alpha = props.opacity or 1.0
        fs = fs or props.fontSize or 20
    elseif target and target.optString then
        fgStr = target:optString("foreground", target:optString("textColor", "#ffffff"))
        alpha = target:optNumber("opacity", 1.0)
        fs = fs or target:optNumber("fontSize", 20)
    else
        fs = fs or 20
    end

    local fgR, fgG, fgB, fgA = state.hexToRgb(fgStr)
    System.drawText(tostring(text), x, y, fs, {fgR, fgG, fgB, (fgA or 1.0) * alpha})
end

function TextMixin.measure(text, fontSize)
    if not text or text == "" then return 0 end
    return System.measureText(tostring(text), fontSize or 20)
end

function TextMixin.getLineHeight(fontSize)
    return fontSize or 20
end

return TextMixin
