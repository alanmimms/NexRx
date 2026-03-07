--[[
  ISG Widget Module
  Modular UI component for Internal Signal Generator control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

local ISG = {}
ISG.__index = ISG

-- Default rules for the ISG widget
setbox.rule {
    id = "isg-defaults",
    tags = {"widget.ISGFrame"},
    priority = -50,
    apply = {
        title = "SIGNAL GEN",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        labelEnabled = "Enabled",
        sliderMin = 0.1,
        sliderMax = 30.0,
    }
}

function ISG.new(state)
    local self = setmetatable({}, ISG)
    self.state = state
    return self
end

function ISG:draw(id, x, y, w, h)
    local prevTags = setbox.getActiveTags()
    setbox.setActiveTags({"widget.Panel", "widget.ISGFrame", "id." .. id})
    
    local bgR, bgG, bgB = theme.hexToRgb(setbox.getString("background", "#242d42"))
    local bR, bG, bB = theme.hexToRgb(setbox.getString("border", "#3b82f6"))
    local radius = setbox.getNumber("borderRadius", 8)
    local alpha = setbox.getNumber("opacity", 1.0)
    local bWidth = setbox.getNumber("borderWidth", 1)
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    
    ui.label(id .. "-title", x + 12, y + 8, setbox.getString("title", "SIGNAL GEN"), {"Title"})
    
    local padding = setbox.getNumber("padding", 12)
    local topMargin = setbox.getNumber("topMargin", 32)
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    -- Frequency Display
    local cx, cy = layout.getCursor()
    ui.frequencyDisplay(id .. "-freq-disp", cx, cy, w - padding * 2, 36, state.isgFrequency, _G.isgFreqEntryText or "", {"IsgControl"})
    layout.newLine(44)

    -- Enabled Toggle
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-en", setbox.getString("labelEnabled", "Enabled"), cx, cy, state.isgEnabled, {"IsgToggle"}, "isgEnabled")
    layout.newLine(28)

    -- Frequency Slider
    cx, cy = layout.getCursor()
    local sMin = setbox.getNumber("sliderMin", 0.1)
    local sMax = setbox.getNumber("sliderMax", 30.0)
    local nF = ui.slider(id .. "-fr-slider", cx, cy, w - padding * 2, sMin, sMax, state.isgFrequency, {"IsgControl"}, "isgFrequency")
    if nF ~= state.isgFrequency then 
        state.isgFrequency = nF 
    end
    layout.newLine(24)
    
    layout.endRegion()
    setbox.setActiveTags(prevTags)
end

return ISG
