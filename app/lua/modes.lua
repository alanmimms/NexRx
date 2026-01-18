--[[
  NexRx Mode Definitions

  Lua owns mode definitions. C++ just accepts integer mode IDs.
  Mode IDs match the C++ Demodulator::Mode enum order.

  Usage:
    local modes = require("modes")
    modes.setMode("USB")
    local name = modes.getModeName()
]]

local Modes = {}

-- Mode ID constants (match C++ Demodulator::Mode enum)
Modes.USB = 0
Modes.LSB = 1
Modes.AM = 2
Modes.CW = 3

-- Mode name lookup tables
Modes.nameToId = {
    USB = 0,
    LSB = 1,
    AM = 2,
    CW = 3,
    -- Allow lowercase too
    usb = 0,
    lsb = 1,
    am = 2,
    cw = 3,
}

Modes.idToName = {
    [0] = "USB",
    [1] = "LSB",
    [2] = "AM",
    [3] = "CW",
}

-- All available mode names (for UI iteration)
Modes.names = {"USB", "LSB", "AM", "CW"}

--- Set demodulator mode by name
-- @param name Mode name (USB, LSB, AM, CW)
-- @return true if mode was set, false if unknown mode
function Modes.setMode(name)
    local modeId = Modes.nameToId[name]
    if modeId and rx and rx.setModeId then
        rx.setModeId(modeId)
        return true
    end
    return false
end

--- Set demodulator mode by ID
-- @param id Mode ID (0-3)
-- @return true if mode was set
function Modes.setModeId(id)
    if id >= 0 and id <= 3 and rx and rx.setModeId then
        rx.setModeId(id)
        return true
    end
    return false
end

--- Get current mode name
-- @return Mode name string
function Modes.getModeName()
    if rx and rx.getModeId then
        local id = rx.getModeId()
        return Modes.idToName[id] or "USB"
    end
    return "USB"
end

--- Get current mode ID
-- @return Mode ID integer
function Modes.getModeId()
    if rx and rx.getModeId then
        return rx.getModeId()
    end
    return 0
end

--- Convert mode ID to name
-- @param id Mode ID
-- @return Mode name or "USB" if invalid
function Modes.toName(id)
    return Modes.idToName[id] or "USB"
end

--- Convert mode name to ID
-- @param name Mode name
-- @return Mode ID or 0 if invalid
function Modes.toId(name)
    return Modes.nameToId[name] or 0
end

print("[modes.lua] Mode definitions loaded")

return Modes
