--[[
  CW Utilities Widget Module
  Modular UI component for CW-specific settings.
  Style and layout driven entirely by SetBox rules.
]]

local state = require("ui.State")
local Label = require("ui.Label")
local Slider = require("ui.Slider")
local layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")

local CWUtils = {}
CWUtils.__index = CWUtils

-- Define default rules for the CW Utils widget (very low priority)
setbox.rule {
    id = "cw-utils-defaults",
    tags = {"widget.CWUtilsFrame"},
    priority = -1000,
    apply = {
        title = "CW SETTINGS",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        labelPitch = "CW Pitch (Hz)",
        minPitch = 300,
        maxPitch = 1200,
    }
}

function CWUtils.new(props)
    local self = setmetatable({}, CWUtils)
    self.rx = props.rx -- Model.rx
    
    -- Initialize widget instances
    self.titleLabel = Label.new()
    self.pitchSlider = Slider.new({
        valueObs = self.rx.CW.pitch,
        propertyName = "rx.CW.pitch"
    })
    self.pitchLabel = Label.new()
    
    return self
end

function CWUtils:draw(id, x, y, w, h, parentLWC)
    local lwc = setbox.newContext({"widget.Panel", "widget.CWUtilsFrame", "id." .. id}, parentLWC)
    
    local bgR, bgG, bgB = state.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = state.hexToRgb(lwc:getString("border"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end
    
    -- Title
    self.titleLabel:draw(id .. "-title", x + 12, y + 8, w - 24, 20, lwc)
    
    local padding = lwc:getNumber("padding")
    local topMargin = lwc:getNumber("topMargin")
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local cx, cy = layout.getCursor()
    self.pitchLabel.getText = function() 
        return string.format("%s: %.0f", lwc:getString("labelPitch"), self.rx.CW.pitch:get())
    end
    self.pitchLabel:draw(id .. "-pitch-label", cx, cy, w - padding * 2, 20, lwc)
    layout.newLine(24)
    
    cx, cy = layout.getCursor()
    self.pitchSlider:draw(id .. "-pitch-slider", cx, cy, w - padding * 2, 24, lwc, 
        lwc:getNumber("minPitch"), lwc:getNumber("maxPitch"), 
        self.rx.CW.pitch:get())
    layout.newLine(32)
    
    layout.endRegion()
end

return CWUtils
