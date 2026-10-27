--[[
  Audio Utilities Widget Module
  Modular UI component for audio settings.
  Style and layout driven by SetBox rules and retained-mode Layout.
]]

local Widget = require("ui.Widget")
local Panel = require("ui.Panel")
local Layout = require("ui.Layout")
local setbox = require("SetBox")
local Label = require("ui.Label")
local Checkbox = require("ui.Checkbox")
local Model = require("Model")

local AudioUtils = Widget.mkType("AudioUtilsFrame", Panel)

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

function AudioUtils:init(def)
    def = def or {}
    def.layoutFunc = def.layoutFunc or Layout.vFlow
    Panel.init(self, def)

    self.rx = def.rx or Model.rx

    self.titleLabel = Label({
        id = self.id .. "-title",
        parent = self,
        prefH = 24,
        stick = "LR",
        margin = { left = 8, right = 8, top = 6, bottom = 4 },
        getText = function() return self.props.title or "AUDIO UTILS" end,
    })

    self.muteCheckbox = Checkbox({
        id = self.id .. "-mute",
        parent = self,
        prefH = 28,
        stick = "LR",
        margin = { left = 8, right = 8, top = 2, bottom = 2 },
        valueObs = self.rx and self.rx.volume and self.rx.volume.muted,
        getText = function() return self.props.labelMute or "Master Mute" end,
        onToggle = function(val) Model.set("rx.volume.muted", val) end,
    })

    self.toneCheckbox = Checkbox({
        id = self.id .. "-tone",
        parent = self,
        prefH = 28,
        stick = "LR",
        margin = { left = 8, right = 8, top = 2, bottom = 6 },
        valueObs = self.rx and self.rx.testToneEnabled,
        getText = function() return self.props.labelTone or "440Hz Test Tone" end,
        onToggle = function(val) Model.set("rx.testToneEnabled", val) end,
    })

    self:add(self.titleLabel)
    self:add(self.muteCheckbox)
    self:add(self.toneCheckbox)
end

function AudioUtils.new(def)
    return AudioUtils(def)
end

return AudioUtils
