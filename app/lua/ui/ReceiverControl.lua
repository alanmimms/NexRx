--[[
  Receiver Control Widget Module
  Retained-mode Panel container composing VFO, Mode, Band, Volume, and RF Gain controls.
]]

local Widget = require("ui.Widget")
local Panel = require("ui.Panel")
local Layout = require("ui.Layout")
local FrequencyDisplay = require("ui.FrequencyDisplay")
local Slider = require("ui.Slider")
local Button = require("ui.Button")
local Label = require("ui.Label")
local Model = require("Model")

local ReceiverControl = Widget.mkType("ReceiverControl", Panel)

function ReceiverControl:init(def)
    def = def or {}
    def.layoutFunc = def.layoutFunc or Layout.vFlow
    Panel.init(self, def)

    self.freqDisplay = FrequencyDisplay({
        id = self.id .. "-freq",
        parent = self,
        prefH = 40,
        stick = "LR",
        margin = 4,
        tags = { "VFOControl" },
        valueObs = Model.rx.VFO.activeValue,
    })

    self.round1k = Button({
        id = self.id .. "-r1k",
        flexW = 1,
        prefH = 26,
        stick = "TLRB",
        margin = 2,
        text = "=000",
        onClick = function() Model.roundFrequency("rx.VFO.activeValue", 1000) end,
    })

    self.round100 = Button({
        id = self.id .. "-r100",
        flexW = 1,
        prefH = 26,
        stick = "TLRB",
        margin = 2,
        text = "=00",
        onClick = function() Model.roundFrequency("rx.VFO.activeValue", 100) end,
    })

    self.roundRow = Widget.Row({
        id = self.id .. "-round",
        parent = self,
        prefH = 30,
        stick = "LR",
        margin = 2,
        kids = { self.round1k, self.round100 },
    })

    self.freqSlider = Slider({
        id = self.id .. "-slider",
        parent = self,
        prefH = 28,
        stick = "LR",
        margin = 4,
        minVal = 0.1e6,
        maxVal = 30.0e6,
        valueObs = Model.rx.VFO.activeValue,
        propertyName = "rx.VFO.activeValue",
    })

    self.modeButtons = {}
    local modeKids = {}
    local modes = { "LSB", "USB", "AM", "CW", "FM" }
    for _, m in ipairs(modes) do
        local btn = Button({
            id = self.id .. "-m-" .. m,
            flexW = 1,
            prefH = 28,
            stick = "TLRB",
            margin = 2,
            text = m,
            onClick = function() Model.set("rx.selectedMode", m) end,
        })
        self.modeButtons[m] = btn
        table.insert(modeKids, btn)
    end

    self.modeRow = Widget.Row({
        id = self.id .. "-modes",
        parent = self,
        prefH = 32,
        stick = "LR",
        margin = 2,
        kids = modeKids,
    })

    self.bandButtons = {}
    local bandKids = {}
    local bands = { "160m", "80m", "40m", "20m", "15m", "10m" }
    for _, b in ipairs(bands) do
        local btn = Button({
            id = self.id .. "-b-" .. b,
            flexW = 1,
            prefH = 28,
            stick = "TLRB",
            margin = 2,
            text = b,
            onClick = function() Model.set("rx.selectedBand", b) end,
        })
        self.bandButtons[b] = btn
        table.insert(bandKids, btn)
    end

    self.bandRow = Widget.Row({
        id = self.id .. "-bands",
        parent = self,
        prefH = 32,
        stick = "LR",
        margin = 2,
        kids = bandKids,
    })

    self.volumeSlider = Slider({
        id = self.id .. "-vol",
        parent = self,
        prefH = 44,
        stick = "LR",
        margin = 4,
        minVal = -60,
        maxVal = 0,
        valueObs = Model.rx.volume.DB,
        propertyName = "rx.volume.DB",
        getText = function() return string.format("Volume: %.0f dB", Model.rx.volume.DB:get()) end,
    })

    self.rfGainSlider = Slider({
        id = self.id .. "-gain",
        parent = self,
        prefH = 44,
        stick = "LR",
        margin = 4,
        minVal = -20,
        maxVal = 60,
        valueObs = Model.rx.RF.gainDB,
        propertyName = "rx.RF.gainDB",
        getText = function() return string.format("RF Gain: %.0f dB", Model.rx.RF.gainDB:get()) end,
    })

    self.labels = {
        mode = Label({ text = "Mode" }),
        band = Label({ text = "Band" }),
        vol  = Label({ text = "Volume" }),
        gain = Label({ text = "RF Gain" }),
    }

    self:add(self.freqDisplay)
    self:add(self.roundRow)
    self:add(self.freqSlider)
    self:add(self.modeRow)
    self:add(self.bandRow)
    self:add(self.volumeSlider)
    self:add(self.rfGainSlider)
end

function ReceiverControl.new(def)
    return ReceiverControl(def)
end

function ReceiverControl:drawSelf()
    local curMode = Model.rx.selectedMode:get()
    for m, btn in pairs(self.modeButtons) do
        local active = (curMode == m)
        btn.props.selected = active
        if btn.lwc then btn.lwc:setTag("state.Active", active) end
    end

    local curBand = Model.rx.selectedBand:get()
    for b, btn in pairs(self.bandButtons) do
        local active = (curBand == b)
        btn.props.selected = active
        if btn.lwc then btn.lwc:setTag("state.Active", active) end
    end

    Panel.drawSelf(self)
end

return ReceiverControl
