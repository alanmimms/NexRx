--[[
  ISG Widget Module
  Modular UI component for Internal Signal Generator control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local AppState = require("app_state")

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
        sliderMin = 0.1e6,
        sliderMax = 30.0e6,
    }
}

function ISG.new(state)
    local self = setmetatable({}, ISG)
    self.state = state
    return self
end

function ISG:draw(id, x, y, w, h)
    local lwc = setbox.newContext({"widget.Panel", "widget.ISGFrame", "id." .. id})
    
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    
    ui.label(id .. "-title", x + 12, y + 8, lwc:getString("title"), {"Title"}, lwc)
    
    local padding = lwc:getNumber("padding")
    local topMargin = lwc:getNumber("topMargin")
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    -- Frequency Display
    local cx, cy = layout.getCursor()
    ui.frequencyDisplay(id .. "-freq-disp", cx, cy, w - padding * 2, 36, state.isgFrequency, _G.isgFreqEntryText or "", {"IsgControl"}, lwc)
    layout.newLine(44)

    -- Enabled Toggle
    cx, cy = layout.getCursor()
    if ui.checkbox(id .. "-en", lwc:getString("labelEnabled"), cx, cy, state.isgEnabled, {"IsgToggle"}, "isgEnabled", lwc) then
        AppState.set("isgEnabled", not state.isgEnabled)
    end
    layout.newLine(28)

    -- Frequency Slider
    cx, cy = layout.getCursor()
    local sMin = lwc:getNumber("sliderMin")
    local sMax = lwc:getNumber("sliderMax")
    local nF = ui.slider(id .. "-fr-slider", cx, cy, w - padding * 2, sMin, sMax, state.isgFrequency, {"IsgControl"}, "isgFrequency", lwc)
    if nF ~= state.isgFrequency then 
        AppState.set("isgFrequency", nF)
    end
    layout.newLine(24)
    
    layout.endRegion()
end

return ISG
