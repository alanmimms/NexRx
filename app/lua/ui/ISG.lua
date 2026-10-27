--[[
  ISG Widget Module
  Modular UI component for Internal Signal Generator control.
  Driven by SetBox rules and retained-mode Layout.
]]

local Widget = require("ui.Widget")
local Panel = require("ui.Panel")
local Label = require("ui.Label")
local Button = require("ui.Button")
local Checkbox = require("ui.Checkbox")
local Slider = require("ui.Slider")
local FrequencyDisplay = require("ui.FrequencyDisplay")
local Layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")

local ISG = Widget.mkType("ISGFrame", Panel)

-- Default rules for the ISG widget (very low priority)
setbox.rule {
    id = "isg-defaults",
    tags = {"widget.ISGFrame"},
    priority = -1000,
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

function ISG:init(def)
    def = def or {}
    def.layoutFunc = def.layoutFunc or Layout.vFlow
    Panel.init(self, def)

    self.ISG = def.ISG or Model.ISG

    self.titleLabel = Label({
        id = self.id .. "-title",
        parent = self,
        prefH = 24,
        stick = "LR",
        margin = { left = 8, right = 8, top = 6, bottom = 4 },
        getText = function() return self.props.title or "SIGNAL GEN" end,
    })

    self.freqDisplay = FrequencyDisplay({
        id = self.id .. "-freq-disp",
        parent = self,
        prefH = 36,
        stick = "LR",
        margin = { left = 8, right = 8, top = 2, bottom = 4 },
        tags = { "IsgControl" },
        valueObs = self.ISG and self.ISG.frequencyHz,
    })

    self.round1k = Button({
        id = self.id .. "-round-1k",
        flexW = 1,
        prefH = 24,
        stick = "TLRB",
        margin = 2,
        text = "=000",
        onClick = function() Model.roundFrequency("ISG.frequencyHz", 1000) end,
    })

    self.round100 = Button({
        id = self.id .. "-round-100",
        flexW = 1,
        prefH = 24,
        stick = "TLRB",
        margin = 2,
        text = "=00",
        onClick = function() Model.roundFrequency("ISG.frequencyHz", 100) end,
    })

    self.roundRow = Widget.Row({
        id = self.id .. "-round-row",
        parent = self,
        prefH = 28,
        stick = "LR",
        margin = { left = 6, right = 6, top = 2, bottom = 2 },
        kids = { self.round1k, self.round100 },
    })

    self.enableCheckbox = Checkbox({
        id = self.id .. "-en",
        parent = self,
        prefH = 28,
        stick = "LR",
        margin = { left = 8, right = 8, top = 2, bottom = 2 },
        valueObs = self.ISG and self.ISG.enabled,
        getText = function() return self.props.labelEnabled or "Enabled" end,
        onToggle = function(val) Model.set("ISG.enabled", val) end,
    })

    self.freqSlider = Slider({
        id = self.id .. "-fr-slider",
        parent = self,
        prefH = 24,
        stick = "LR",
        margin = { left = 8, right = 8, top = 2, bottom = 6 },
        minVal = self.props.sliderMin or 0.1e6,
        maxVal = self.props.sliderMax or 30.0e6,
        valueObs = self.ISG and self.ISG.frequencyHz,
        propertyName = "ISG.frequencyHz",
    })

    self:add(self.titleLabel)
    self:add(self.freqDisplay)
    self:add(self.roundRow)
    self:add(self.enableCheckbox)
    self:add(self.freqSlider)
end

function ISG.new(def)
    return ISG(def)
end

return ISG
