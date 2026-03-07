--[[
  S-Meter Widget
  Modular UI component for signal strength display.
  Style and layout driven entirely by SetBox rules.
]]

local setbox = require("setbox")
local Label = require("ui.Label")

local SMeterWidget = {}
SMeterWidget.__index = SMeterWidget

-- Default rules for S-Meter widget (very low priority)
setbox.rule {
    id = "smeter-defaults",
    tags = {"widget.SMeter"},
    priority = -1000,
    apply = {
        background = "#0f172a",
        colorWeak = "#22c55e",
        colorMid = "#eab308",
        colorStrong = "#ef4444",
        colorOff = "#1e293b",
        borderRadius = 4,
        padding = 4,
        barCount = 12,
        barGap = 2,
        opacity = 1.0,
    }
}

function SMeterWidget.new()
    local self = setmetatable({}, SMeterWidget)
    self.titleLabel = Label.new()
    return self
end

function SMeterWidget:draw(id, x, y, w, h, reading, parentLWC)
    local lwc = setbox.newContext({"widget.SMeter", "id." .. id}, parentLWC)
    
    -- Properties from rules
    local bgR, bgG, bgB = require("ui.widgets").hexToRgb(lwc:getString("background"))
    local weakR, weakG, weakB = require("ui.widgets").hexToRgb(lwc:getString("colorWeak"))
    local midR, midG, midB = require("ui.widgets").hexToRgb(lwc:getString("colorMid"))
    local strongR, strongG, strongB = require("ui.widgets").hexToRgb(lwc:getString("colorStrong"))
    local offR, offG, offB = require("ui.widgets").hexToRgb(lwc:getString("colorOff"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local pad = lwc:getNumber("padding")
    local barCount = lwc:getNumber("barCount")
    local barGap = lwc:getNumber("barGap")

    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)

    local barW = (w - pad * 2 - barGap * (barCount - 1)) / barCount
    local barH = h - pad * 2
    
    for i = 0, barCount - 1 do
        local barX = x + pad + i * (barW + barGap)
        -- Map 12 bars: 
        -- 0-8: S1-S9 (thresholds 1.0 to 9.0)
        -- 9-11: S9+20, S9+40, S9+60 (thresholds 12.33, 15.66, 19.0)
        local barThreshold = (i < 9) and (i + 1) or (9 + (i - 8) * (20/6))

        if reading.sUnits >= barThreshold then
        local r, g, b = weakR, weakG, weakB
        if i >= 9 then r, g, b = strongR, strongG, strongB
        elseif i >= 6 then r, g, b = midR, midG, midB end
        drawRect(barX, y + pad, barW, barH, r, g, b, alpha)
    else
        drawRect(barX, y + pad, barW, barH, offR, offG, offB, alpha)
    end
end

    local ty = y + h + 4
    local sW = measureText(reading.sText)
    local dW = measureText(reading.dBmText)
    local startX = x + (w - (sW + 10 + dW)) / 2
    drawText(startX, ty, reading.sText, 0.9, 0.9, 0.95, alpha)
    drawText(startX + sW + 10, ty, reading.dBmText, 0.5, 0.5, 0.55, alpha)
end

return SMeterWidget
