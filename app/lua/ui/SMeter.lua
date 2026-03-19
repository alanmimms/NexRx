--[[
  SMeter - Calculations and UI Widget
  
  Provides both the logic for converting RMS voltage to S-units/dBm
  and the modular UI component for display.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Label = require("ui.Label")

local SMeter = {}
SMeter.__index = SMeter

-- =============================================================================
-- Calculation Logic
-- =============================================================================

SMeter.S9_DBM = -73.0         -- S9 level in dBm
SMeter.DB_PER_S_UNIT = 6.0    -- dB per S-unit
SMeter.DBM_FLOOR = -127.0     -- Minimum dBm (noise floor)
SMeter.RMS_FLOOR = 1e-9       -- Minimum RMS threshold
SMeter.RMS_TO_DBM_OFFSET = 13.0 - 40.0

function SMeter.rmsTodBm(rms)
    if rms <= SMeter.RMS_FLOOR then return SMeter.DBM_FLOOR end
    return 20.0 * math.log10(rms) + SMeter.RMS_TO_DBM_OFFSET
end

function SMeter.dBmToSUnits(dBm)
    local sUnits = (dBm - SMeter.S9_DBM) / SMeter.DB_PER_S_UNIT + 9.0
    return math.max(0, sUnits)
end

function SMeter.dBOverS9(dBm)
    return dBm - SMeter.S9_DBM
end

function SMeter.formatSUnits(sUnits, dBm)
    if sUnits < 9 then
        return string.format("S%.0f", math.max(1, math.floor(sUnits + 0.5)))
    else
        local dBover = SMeter.dBOverS9(dBm)
        return string.format("S9+%.0f", math.max(0, dBover))
    end
end

function SMeter.formatdBm(dBm)
    return string.format("%.0f dBm", dBm)
end

function SMeter.getReading()
    local rms = 0
    if rx and rx.getSignalRms then
        rms = rx.getSignalRms()
    end

    local dBm = SMeter.rmsTodBm(rms)
    local sUnits = SMeter.dBmToSUnits(dBm)
    local dBover = SMeter.dBOverS9(dBm)

    return {
        rms = rms,
        dBm = dBm,
        sUnits = sUnits,
        dBOverS9 = dBover,
        sText = SMeter.formatSUnits(sUnits, dBm),
        dBmText = SMeter.formatdBm(dBm)
    }
end

-- =============================================================================
-- UI Widget Logic
-- =============================================================================

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
