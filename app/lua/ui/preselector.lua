--[[
  Preselector Widget Module
  Modular UI component for RX preselector control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

local Preselector = {}
Preselector.__index = Preselector

--- Create a new Preselector widget instance
-- @param state The reactive state proxy (usually 'state' from main.lua)
function Preselector.new(state)
    local self = setmetatable({}, Preselector)
    self.state = state
    return self
end

--- Draw the preselector widget
-- @param id Unique ID for this instance
-- @param x, y Position
-- @param w, h Size
function Preselector:draw(id, x, y, w, h)
    -- 1. Get theme colors for the frame
    local style = theme.getStyle({"widget.Panel", "widget.PreselectorFrame"})
    
    -- 2. Draw surrounding frame
    drawRoundedRect(x, y, w, h, style.borderRadius or 6, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, 1)
    
    -- 3. Draw Frame Label
    ui.label(id .. "-title", x + 12, y + 8, "PRESELECTOR", {"Title"})
    
    -- 4. Setup inner layout region
    local padding = 12
    local topMargin = 32
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    local cx, cy
    
    -- Enabled Toggle
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-en", "Enabled", cx, cy, state.preselectorEnabled, {"PreToggle"}, "preselectorEnabled")
    layout.newLine(28)
    
    -- Auto-tune Toggle
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-auto", "Auto-tune", cx, cy, state.preselectorAuto, {"PreselAuto"}, "preselectorAuto")
    layout.newLine(28)
    
    -- Inductor Toggle
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-l1", "Inductor L1", cx, cy, state.preselL1, {"PreselToggle"}, "preselL1")
    layout.newLine(28)
    
    -- Capacitor Bank Grid
    layout.newLine(4) -- Extra breathing room
    for i = 0, 10 do
        if i % 4 == 0 and i > 0 then layout.newLine(28) end
        if i % 4 == 0 then layout.beginHorizontal(4) end
        
        local cid = "preselC" .. i
        local bx, by = layout.reserveSpace(45, 24)
        ui.checkbox(id .. "-" .. cid, "C" .. i, bx, by, state[cid], {"PreselToggle"}, cid)
        
        if i % 4 == 3 or i == 10 then layout.endHorizontal() end
    end
    
    layout.endRegion()
end

return Preselector
