--[[
  CW Utilities Widget Module
  Modular UI component for CW-specific settings.
  Style and layout driven by SetBox rules and retained-mode Layout.
]]

local Widget = require("ui.Widget")
local Panel = require("ui.Panel")
local Label = require("ui.Label")
local Slider = require("ui.Slider")
local Layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")

local CWUtils = Widget.mkType("CWUtilsFrame", Panel)

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

function CWUtils:init(def)
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
        getText = function() return self.props.title or "CW SETTINGS" end,
    })

    self.pitchLabel = Label({
        id = self.id .. "-pitch-label",
        parent = self,
        prefH = 20,
        stick = "LR",
        margin = { left = 8, right = 8, top = 2, bottom = 2 },
        getText = function()
            local pitch = self.rx and self.rx.CW and self.rx.CW.pitch and self.rx.CW.pitch:get() or 700
            return string.format("%s: %.0f", self.props.labelPitch or "CW Pitch (Hz)", pitch)
        end,
    })

    self.pitchSlider = Slider({
        id = self.id .. "-pitch-slider",
        parent = self,
        prefH = 24,
        stick = "LR",
        margin = { left = 8, right = 8, top = 2, bottom = 6 },
        minVal = self.props.minPitch or 300,
        maxVal = self.props.maxPitch or 1200,
        valueObs = self.rx and self.rx.CW and self.rx.CW.pitch,
        propertyName = "rx.CW.pitch",
    })

    self:add(self.titleLabel)
    self:add(self.pitchLabel)
    self:add(self.pitchSlider)
end

function CWUtils.new(def)
    return CWUtils(def)
end

return CWUtils
