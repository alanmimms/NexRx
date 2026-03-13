--[[
  Audio Utilities Widget Module
  Modular UI component for audio settings.
  Style and layout driven entirely by SetBox rules.
]]

local layout = require("ui.Layout")
local setbox = require("SetBox")
local state = require("ui.State")
local Label = require("ui.Label")
local Checkbox = require("ui.Checkbox")
local Model = require("Model")

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
        height = function(lwc)
            -- TopMargin (32) + Mute (28) + Tone (28) + Padding (12) + Spacing
            return lwc:getNumber("topMargin") + 28 + 28 + lwc:getNumber("padding") + 16
        end,
        labelMute = "Master Mute",
        labelFilters = "Demod Filters",
        labelTone = "440Hz Test Tone",
    }
}

function AudioUtils.new(props)
    local self = setmetatable({}, AudioUtils)
    self.rx = props.rx -- Model.rx
    
    -- Initialize widget instances
    self.titleLabel = Label.new()
    self.muteCheckbox = Checkbox.new({
        onToggle = function(val) Model.set("rx.volume.muted", val) end
    })
    self.toneCheckbox = Checkbox.new({
        onToggle = function(val) Model.set("rx.testToneEnabled", val) end
    })
    
    return self
end

function AudioUtils:draw(id, x, y, w, h, parentLWC)
    local lwc = setbox.newContext({"widget.Panel", "widget.AudioUtilsFrame", "id." .. id}, parentLWC)
    
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
    self.muteCheckbox.getText = function() return lwc:getString("labelMute") end
    self.muteCheckbox:draw(id .. "-mute", cx, cy, w - padding * 2, 28, lwc, self.rx.volume.muted:get())
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    self.toneCheckbox.getText = function() return lwc:getString("labelTone") end
    local toneVal = self.rx.testToneEnabled:get()
    if state.isHot(id .. "-tone") then
        -- print("[AudioUtils] toneVal=" .. tostring(toneVal))
    end
    self.toneCheckbox:draw(id .. "-tone", cx, cy, w - padding * 2, 28, lwc, toneVal)
    layout.newLine(28)
    
    layout.endRegion()
end

return AudioUtils
