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

local function log10(x)
    if math.log10 then
        return math.log10(x)
    end
    return math.log(x) / math.log(10)
end

function SMeter.rmsTodBm(rms)
    if rms <= SMeter.RMS_FLOOR then return SMeter.DBM_FLOOR end
    return 20.0 * log10(rms) + SMeter.RMS_TO_DBM_OFFSET
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
    self.peakUnits = 0.0
    self.lastPeakTime = 0.0
end

function SMeter:calcMetrics()
    if (self.props.prefW or 0) == 0 then self.props.prefW = 160 end
    if (self.props.prefH or 0) == 0 then self.props.prefH = 34 end
end

function SMeter:drawSelf(reading)
    local w = self.props.w or 0
    reading = reading or SMeter.getReading()

    -- Dual-decay peak tracking
    local now = os.clock()
    local dt = now - (self.lastPeakTime or now)
    self.lastPeakTime = now
    if (reading.sUnits or 0) >= (self.peakUnits or 0) then
        self.peakUnits = reading.sUnits
    else
        -- Slow decay (~2 S-units per second)
        self.peakUnits = math.max(reading.sUnits, (self.peakUnits or 0) - dt * 2.0)
    end

    local bgR, bgG, bgB = state.hexToRgb(self.props.background or "#0f172a")
    local weakR, weakG, weakB = state.hexToRgb(self.props.colorWeak or "#22c55e")
    local midR, midG, midB = state.hexToRgb(self.props.colorMid or "#eab308")
    local strongR, strongG, strongB = state.hexToRgb(self.props.colorStrong or "#ef4444")
    local offR, offG, offB = state.hexToRgb(self.props.colorOff or "#1e293b")
    local alpha = self.props.opacity or 1.0
    local pad = self.props.padding or 2
    local barCount = self.props.barCount or 15
    local barGap = self.props.barGap or 2

    local barActualH = 14
    local barY = 2
    System.drawRoundedRect(0, barY, w, barActualH, 2, {bgR, bgG, bgB, alpha * 0.7})

    local barW = (w - pad * 2 - barGap * (barCount - 1)) / barCount
    local barH = barActualH - pad * 2

    for i = 0, barCount - 1 do
        local barX = pad + i * (barW + barGap)
        local barThreshold = (i < 9) and (i + 1) or (9 + (i - 8) * (30 / (barCount - 9)))

        local r, g, b = weakR, weakG, weakB
        if i >= 9 then r, g, b = strongR, strongG, strongB
        elseif i >= 6 then r, g, b = midR, midG, midB end

        if reading.sUnits >= barThreshold then
            -- Solidly filled with high saturation up to instantaneous level
            System.drawRect(barX, barY + pad, barW, barH, {r, g, b, alpha})
        elseif (self.peakUnits or 0) >= barThreshold then
            -- Hollow box outline up to slow-decay peak
            System.drawRectLines(barX, barY + pad, barW, barH, 1, {r, g, b, alpha * 0.7})
        else
            -- Off segment
            System.drawRect(barX, barY + pad, barW, barH, {offR, offG, offB, alpha * 0.4})
        end
    end

    local ty = barY + barActualH + 3
    local sText = reading.sText or "S0"
    local dbmText = reading.dBmText or "-120 dBm"
    System.drawText(sText, 2, ty, 13, {0.9, 0.9, 0.95, alpha})
    local dbmW = System.measureText(dbmText, 13) or (#dbmText * 8)
    System.drawText(dbmText, w - dbmW - 2, ty, 13, {0.55, 0.55, 0.6, alpha})
end

return SMeter
