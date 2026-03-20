--[[
  SMeter - Calculations and UI Widget
  
  Provides both the logic for converting RMS voltage to S-units/dBm
  and the modular UI component for display.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")

local SMeter = Widget.mkType("SMeter")

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
    elseif _G.rx and _G.rx.getStats then
        local stats = _G.rx.getStats()
        rms = stats.rms or 0
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
        opacity = 1.0
    }
}

function SMeter:init(def)
    Widget.init(self, def)
end

function SMeter:calcMetrics()
    if self.metrics.prefW == 0 then self.metrics.prefW = 200 end
    if self.metrics.prefH == 0 then self.metrics.prefH = 66 end
end

function SMeter:drawSelf(reading)
    local w, h = self.props.w, self.props.h
    local lwc = self.lwc
    
    reading = reading or SMeter.getReading()

    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#0f172a"))
    local weakR, weakG, weakB = state.hexToRgb(lwc:optString("colorWeak", "#22c55e"))
    local midR, midG, midB = state.hexToRgb(lwc:optString("colorMid", "#eab308"))
    local strongR, strongG, strongB = state.hexToRgb(lwc:optString("colorStrong", "#ef4444"))
    local offR, offG, offB = state.hexToRgb(lwc:optString("colorOff", "#1e293b"))
    local radius = lwc:optNumber("borderRadius", 4)
    local alpha = lwc:optNumber("opacity", 1.0)
    local pad = lwc:optNumber("padding", 4)
    local barCount = lwc:optNumber("barCount", 12)
    local barGap = lwc:optNumber("barGap", 2)

    local title = lwc:optString("title", "S-METER")
    System.drawText(title, 0, 0, 16, {0.7, 0.7, 0.8, alpha})
    
    local barY = 18
    local barActualH = 28
    System.drawRoundedRect(0, barY, w, barActualH, radius, {bgR, bgG, bgB, alpha})

    local barW = (w - pad * 2 - barGap * (barCount - 1)) / barCount
    local barH = barActualH - pad * 2
    
    for i = 0, barCount - 1 do
        local barX = pad + i * (barW + barGap)
        local barThreshold = (i < 9) and (i + 1) or (9 + (i - 8) * (20/6))

        if reading.sUnits >= barThreshold then
            local r, g, b = weakR, weakG, weakB
            if i >= 9 then r, g, b = strongR, strongG, strongB
            elseif i >= 6 then r, g, b = midR, midG, midB end
            System.drawRect(barX, barY + pad, barW, barH, {r, g, b, alpha})
        else
            System.drawRect(barX, barY + pad, barW, barH, {offR, offG, offB, alpha})
        end
    end

    local ty = barY + barActualH + 4
    local sW = System.measureText(reading.sText, 14)
    local dW = System.measureText(reading.dBmText, 14)
    local startX = (w - (sW + 10 + dW)) / 2
    System.drawText(reading.sText, startX, ty, 14, {0.9, 0.9, 0.95, alpha})
    System.drawText(reading.dBmText, startX + sW + 10, ty, 14, {0.5, 0.5, 0.55, alpha})
end

return SMeter
