--[[
  NexRx Mode Definitions

  Lua owns mode definitions. C++ just accepts integer mode IDs.
  Mode IDs match the C++ Demodulator::Mode enum order.

  SINGLE SOURCE OF TRUTH: The 'names' array below defines all modes.
  All other tables and constants are derived from it.

  Usage:
    local modes = require("modes")
    modes.setMode("USB")
    local name = modes.getModeName()
]]

local Modes = {}

-- SINGLE SOURCE OF TRUTH: Mode names in ID order (index 1 = ID 0)
-- Order must match C++ Demodulator::Mode enum
Modes.names = {"USB", "LSB", "AM", "CW", "BYPASS"}

-- Derive all other tables from names
Modes.nameToId = {}
Modes.idToName = {}

for id, name in ipairs(Modes.names) do
    local modeId = id - 1  -- Lua arrays are 1-based, mode IDs are 0-based
    Modes.nameToId[name] = modeId
    Modes.nameToId[name:lower()] = modeId  -- Allow lowercase
    Modes.idToName[modeId] = name
    Modes[name] = modeId  -- Constants: Modes.USB = 0, etc.
end

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
-- @param id Mode ID (0 to #names-1)
-- @return true if mode was set
function Modes.setModeId(id)
    if id >= 0 and id < #Modes.names and rx and rx.setModeId then
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
