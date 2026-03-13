--[[
  S-Meter Widget
  Modular UI component for signal strength display.
  Style and layout driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Label = require("ui.Label")

local SMeter = {}
SMeter.__index = SMeter

-- Default rules for S-Meter widget (very low priority)
setbox.rule {
    id = "smeter-defaults",
    tags = {"widget.SMeter"},
    priority = -1000,
    apply = {
        title = "S-METER",
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
        height = function(lwc)
            return 18 + 28 + 20 -- Title + Bar + Text
        end
    }
}

function SMeter.new()
    local self = setmetatable({}, SMeter)
    self.titleLabel = Label.new()
    return self
end

function SMeter:draw(id, x, y, w, h, parentLWC, reading)
    local lwc = setbox.newContext({"widget.SMeter", "id." .. id}, parentLWC)
    
    -- Properties from rules
    local bgR, bgG, bgB = state.hexToRgb(lwc:getString("background"))
    local weakR, weakG, weakB = state.hexToRgb(lwc:getString("colorWeak"))
    local midR, midG, midB = state.hexToRgb(lwc:getString("colorMid"))
    local strongR, strongG, strongB = state.hexToRgb(lwc:getString("colorStrong"))
    local offR, offG, offB = state.hexToRgb(lwc:getString("colorOff"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local pad = lwc:getNumber("padding")
    local barCount = lwc:getNumber("barCount")
    local barGap = lwc:getNumber("barGap")

    -- Draw Title
    self.titleLabel.getText = function() return lwc:getString("title") end
    self.titleLabel:draw(id .. "-title", x, y, w, 18, lwc)
    
    local barY = y + 18
    local barActualH = 28
    drawRoundedRect(x, barY, w, barActualH, radius, bgR, bgG, bgB, alpha)

    local barW = (w - pad * 2 - barGap * (barCount - 1)) / barCount
    local barH = barActualH - pad * 2
    
    for i = 0, barCount - 1 do
        local barX = x + pad + i * (barW + barGap)
        local barThreshold = (i < 9) and (i + 1) or (9 + (i - 8) * (20/6))

        if reading.sUnits >= barThreshold then
            local r, g, b = weakR, weakG, weakB
            if i >= 9 then r, g, b = strongR, strongG, strongB
            elseif i >= 6 then r, g, b = midR, midG, midB end
            drawRect(barX, barY + pad, barW, barH, r, g, b, alpha)
        else
            drawRect(barX, barY + pad, barW, barH, offR, offG, offB, alpha)
        end
    end

    local ty = barY + barActualH + 4
    local sW = measureText(reading.sText)
    local dW = measureText(reading.dBmText)
    local startX = x + (w - (sW + 10 + dW)) / 2
    drawText(startX, ty, reading.sText, 0.9, 0.9, 0.95, alpha)
    drawText(startX + sW + 10, ty, reading.dBmText, 0.5, 0.5, 0.55, alpha)
end

return SMeter

