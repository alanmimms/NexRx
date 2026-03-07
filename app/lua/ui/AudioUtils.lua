--[[
  Audio Utilities Widget Module
  Modular UI component for audio settings.
  Style and layout driven entirely by SetBox rules.
]]

local ui = require("ui.Widgets")
local layout = require("ui.Layout")
local AppState = require("AppState")
local setbox = require("SetBox")

local AudioUtils = {}
AudioUtils.__index = AudioUtils

-- Define default rules for the Audio Utils widget (very low priority)
setbox.rule {
    id = "audio-utils-defaults",
    tags = {"widget.AudioUtilsFrame"},
    priority = -1000,
    apply = {
        title = "AUDIO UTILS",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        labelMute = "Master Mute",
        labelFilters = "Demod Filters",
        labelTone = "440Hz Test Tone",
    }
}

function AudioUtils.new(state)
    local self = setmetatable({}, AudioUtils)
    self.state = state
    
    -- Initialize widget instances
    self.titleLabel = ui.Label.new()
    self.muteCheckbox = ui.Checkbox.new({
        onToggle = function(val) AppState.set("muteEnabled", val) end
    })
    self.filterCheckbox = ui.Checkbox.new({
        onToggle = function(val) AppState.set("demodFilterEnabled", val) end
    })
    self.toneCheckbox = ui.Checkbox.new({
        onToggle = function(val) AppState.set("testToneEnabled", val) end
    })
    
    return self
end

function AudioUtils:draw(id, x, y, w, h)
    local lwc = setbox.newContext({"widget.Panel", "widget.AudioUtilsFrame", "id." .. id})
    
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end
    
    -- Title
    self.titleLabel:draw(id .. "-title", x + 12, y + 8, lwc)
    
    local padding = lwc:getNumber("padding")
    local topMargin = lwc:getNumber("topMargin")
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    local cx, cy = layout.getCursor()
    self.muteCheckbox.getText = function() return lwc:getString("labelMute") end
    self.muteCheckbox:draw(id .. "-mute", cx, cy, state.muteEnabled, lwc)
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    self.filterCheckbox.getText = function() return lwc:getString("labelFilters") end
    self.filterCheckbox:draw(id .. "-filters", cx, cy, state.demodFilterEnabled, lwc)
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    self.toneCheckbox.getText = function() return lwc:getString("labelTone") end
    self.toneCheckbox:draw(id .. "-tone", cx, cy, state.testToneEnabled, lwc)
    layout.newLine(28)
    
    layout.endRegion()
end

return AudioUtils
