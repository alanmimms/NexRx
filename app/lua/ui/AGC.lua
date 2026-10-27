--[[
  AGC Widget Module
  Modular UI component for Automatic Gain Control.
  Style and layout driven by SetBox rules and retained-mode Layout.
]]

local Widget = require("ui.Widget")
local Panel = require("ui.Panel")
local Label = require("ui.Label")
local Button = require("ui.Button")
local Layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")

local AGC = Widget.mkType("AGCFrame", Panel)

-- Default rules for the AGC widget (very low priority)
setbox.rule {
    id = "agc-defaults",
    tags = {"widget.AGCFrame"},
    priority = -1000,
    apply = {
        title = "AGC",
        text = "AGC",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        buttonHeight = 24,
        buttonGap = 4,
        labelOff = "Off",
        labelSlow = "Slow",
        labelMed = "Med",
        labelFast = "Fast",
    }
}

function AGC:init(def)
    def = def or {}
    def.layoutFunc = def.layoutFunc or Layout.vFlow
    Panel.init(self, def)

    self.AGC = def.AGC or (Model.rx and Model.rx.AGC)

    self.titleLabel = Label({
        id = self.id .. "-title",
        parent = self,
        prefH = 24,
        stick = "LR",
        margin = { left = 8, right = 8, top = 6, bottom = 4 },
        getText = function() return self.props.title or "AGC" end,
    })

    local modes = {
        { id = "off",  prop = "labelOff",  default = "Off",  val = 0 },
        { id = "slow", prop = "labelSlow", default = "Slow", val = 3 },
        { id = "med",  prop = "labelMed",  default = "Med",  val = 2 },
        { id = "fast", prop = "labelFast", default = "Fast", val = 1 },
    }

    self.modeButtons = {}
    local btnRowKids = {}
    for _, m in ipairs(modes) do
        local btn = Button({
            id = self.id .. "-mode-" .. m.id,
            flexW = 1,
            prefH = self.props.buttonHeight or 24,
            stick = "TLRB",
            margin = 2,
            getText = function() return self.props[m.prop] or m.default end,
            onClick = function() Model.set("rx.AGC.mode", m.val) end,
        })
        self.modeButtons[m.id] = { widget = btn, val = m.val }
        table.insert(btnRowKids, btn)
    end

    self.buttonRow = Widget.Row({
        id = self.id .. "-modes",
        parent = self,
        prefH = 30,
        stick = "LR",
        margin = { left = 6, right = 6, top = 2, bottom = 6 },
        kids = btnRowKids,
    })

    self:add(self.titleLabel)
    self:add(self.buttonRow)
end

function AGC.new(def)
    return AGC(def)
end

function AGC:drawSelf()
    local currentMode = self.AGC and self.AGC.mode and self.AGC.mode:get() or 0
    for _, entry in pairs(self.modeButtons) do
        local active = (currentMode == entry.val)
        entry.widget.props.selected = active
        if entry.widget.lwc then
            entry.widget.lwc:setTag("state.Active", active)
        end
    end
    Panel.drawSelf(self)
end

return AGC
