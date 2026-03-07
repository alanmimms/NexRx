--[[
  ISG Widget Module
  Modular UI component for Internal Signal Generator control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

local ISG = {}
ISG.__index = ISG

function ISG.new(state)
    local self = setmetatable({}, ISG)
    self.state = state
    return self
end

function ISG:draw(id, x, y, w, h)
    local style = theme.getStyle({"widget.Panel", "widget.ISGFrame"})
    
    drawRoundedRect(x, y, w, h, style.borderRadius or 6, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, 1)
    
    ui.label(id .. "-title", x + 12, y + 8, "SIGNAL GEN", {"Title"})
    
    local padding = 12
    local topMargin = 32
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    -- Frequency Display
    local cx, cy = layout.getCursor()
    -- Note: isgFreqEntryText should probably move to state or widget internal data eventually
    -- For now, we'll assume it's passed or handled globally like in main.lua
    ui.frequencyDisplay(id .. "-freq-disp", cx, cy, w - padding * 2, 36, state.isgFrequency, _G.isgFreqEntryText or "", {"IsgControl"})
    layout.newLine(44)

    -- Enabled Toggle
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-en", "Enabled", cx, cy, state.isgEnabled, {"IsgToggle"}, "isgEnabled")
    layout.newLine(28)

    -- Frequency Slider
    cx, cy = layout.getCursor()
    ui.slider(id .. "-fr-slider", cx, cy, w - padding * 2, 0.1, 30.0, state.isgFrequency, {"IsgControl"}, "isgFrequency")
    layout.newLine(24)
    
    layout.endRegion()
end

return ISG
