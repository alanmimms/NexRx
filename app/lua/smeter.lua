--[[
  NexRx S-Meter Calculations

  Lua owns all S-meter calculations. C++ only provides raw RMS.
  This module converts RMS voltage to dBm and S-units.

  S-meter standard (IARU Region 1):
  - S9 = -73 dBm (50 ohm, HF)
  - Each S-unit = 6 dB
  - Above S9: report as S9+dB

  Usage:
    local smeter = require("smeter")
    local reading = smeter.getReading()
]]

local SMeter = {}

-- Reference levels (can be customized for calibration)
SMeter.S9_DBM = -73.0         -- S9 level in dBm
SMeter.DB_PER_S_UNIT = 6.0    -- dB per S-unit
SMeter.DBM_FLOOR = -127.0     -- Minimum dBm (noise floor)
SMeter.RMS_FLOOR = 1e-9       -- Minimum RMS threshold

-- Conversion factor: dBm = 20*log10(RMS) + offset
-- For 50 ohm: offset = 10*log10(1/(50*0.001)) = 13 dB
SMeter.RMS_TO_DBM_OFFSET = 13.0

--- Convert RMS voltage to dBm
-- @param rms RMS voltage level
-- @return dBm value
function SMeter.rmsTodBm(rms)
    if rms <= SMeter.RMS_FLOOR then
        return SMeter.DBM_FLOOR
    end
    return 20.0 * math.log10(rms) + SMeter.RMS_TO_DBM_OFFSET
end

--- Convert dBm to S-units
-- @param dBm Power level in dBm
-- @return S-units (can be > 9 for strong signals)
function SMeter.dBmToSUnits(dBm)
    -- S = (dBm - S9_DBM) / DB_PER_S_UNIT + 9
    local sUnits = (dBm - SMeter.S9_DBM) / SMeter.DB_PER_S_UNIT + 9.0
    return math.max(0, sUnits)
end

--- Get dB over S9
-- @param dBm Power level in dBm
-- @return dB above S9 (negative if below S9)
function SMeter.dBOverS9(dBm)
    return dBm - SMeter.S9_DBM
end

--- Format S-meter reading as text
-- @param sUnits S-units value
-- @param dBm dBm value (for dB over S9 display)
-- @return Formatted string like "S7" or "S9+20"
function SMeter.formatSUnits(sUnits, dBm)
    if sUnits < 9 then
        return string.format("S%.0f", math.max(1, math.floor(sUnits + 0.5)))
    else
        local dBover = SMeter.dBOverS9(dBm)
        return string.format("S9+%.0f", math.max(0, dBover))
    end
end

--- Format dBm reading as text
-- @param dBm Power level in dBm
-- @return Formatted string like "-85 dBm"
function SMeter.formatdBm(dBm)
    return string.format("%.0f dBm", dBm)
end

--- Get current S-meter reading from hardware
-- @return Table with rms, dBm, sUnits, dBOverS9, sText, dBmText
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

--- Get individual values (for backward compatibility)
-- @return rms, dBm, sUnits, dBOverS9
function SMeter.getValues()
    local r = SMeter.getReading()
    return r.rms, r.dBm, r.sUnits, r.dBOverS9
end


return SMeter
