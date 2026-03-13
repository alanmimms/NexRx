--[[
  Receiver Control Widget Module
  Exploded Hierarchy Version: Let Container solver handle parts.
]]

local layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")
local container = require("ui.Container")

local ReceiverControl = {}
ReceiverControl.__index = ReceiverControl

function ReceiverControl.new()
    local self = setmetatable({}, ReceiverControl)
    
    local FrequencyDisplay = require("ui.FrequencyDisplay")
    local Slider = require("ui.Slider")
    local Button = require("ui.Button")
    local Label = require("ui.Label")
    
    self.freqDisplay = FrequencyDisplay.new({ valueObs = Model.rx.VFO.activeValue })
    self.freqSlider = Slider.new({ valueObs = Model.rx.VFO.activeValue })
    self.volumeSlider = Slider.new({ valueObs = Model.rx.volume.DB, propertyName = "rx.volume.DB" })
    self.rfGainSlider = Slider.new({ valueObs = Model.rx.RF.gainDB, propertyName = "rx.RF.gainDB" })
    
    self.round1k = Button.new({ onClick = function() Model.roundFrequency("rx.VFO.activeValue", 1000) end })
    self.round100 = Button.new({ onClick = function() Model.roundFrequency("rx.VFO.activeValue", 100) end })

    self.modeButtons = {}
    local modes = {"LSB", "USB", "AM", "CW", "FM"}
    for _, m in ipairs(modes) do
        self.modeButtons[m] = Button.new({ getText = function() return m end, onClick = function() Model.set("rx.selectedMode", m) end })
    end

    self.bandButtons = {}
    local bands = {"160m","80m","40m","20m","15m","10m"}
    for _, b in ipairs(bands) do
        self.bandButtons[b] = Button.new({ getText = function() return b end, onClick = function() Model.set("rx.selectedBand", b) end })
    end

    self.labels = {
        mode = Label.new({ getText = function() return "Mode" end }),
        band = Label.new({ getText = function() return "Band" end }),
        vol = Label.new({ getText = function() return "Volume" end }),
        gain = Label.new({ getText = function() return "RF Gain" end }),
    }

    return self
end

function ReceiverControl:draw(id, x, y, w, h, parentLWC)
    -- print(string.format("  [ReceiverControl] draw('%s', w=%d, h=%d)", id, w, h))
    local lwc = setbox.newContext({"widget.ReceiverControl", "id." .. id}, parentLWC)
    
    local pad = 8
    local inner = { x = x + pad, y = y + pad, w = w - pad*2, h = h - pad*2 }
    local regions = container.solveDynamicSublayout(inner, id)
    
    -- Draw parts based on solved regions
    local rFreq = regions["id-rx-control-freq"]
    if rFreq then self.freqDisplay:draw(id .. "-freq", rFreq.x, rFreq.y, rFreq.w, rFreq.h, _G.freqEntryText, _G.freqEntryCursor, {"VFOControl"}, lwc) end

    local rRound = regions["id-rx-control-round"]
    if rRound then
        local bw = (rRound.w - 4) / 2
        self.round1k:draw(id .. "-r1k", rRound.x, rRound.y, bw, rRound.h, {}, lwc)
        self.round100:draw(id .. "-r100", rRound.x + bw + 4, rRound.y, bw, rRound.h, {}, lwc)
    end

    local rSlider = regions["id-rx-control-slider"]
    if rSlider then self.freqSlider:draw(id .. "-slider", rSlider.x, rSlider.y, rSlider.w, 0.1e6, 30.0e6, Model.rx.VFO.activeValue:peek(), lwc) end

    local rModes = regions["id-rx-control-modes"]
    if rModes then
        local mList = {"LSB", "USB", "AM", "CW", "FM"}
        local bw = (rModes.w - (#mList-1)*4) / #mList
        for i, m in ipairs(mList) do
            local active = Model.rx.selectedMode:get() == m and {"state.Active"} or {}
            self.modeButtons[m]:draw(id .. "-m-" .. m, rModes.x + (i-1)*(bw+4), rModes.y, bw, rModes.h, active, lwc)
        end
    end

    local rBands = regions["id-rx-control-bands"]
    if rBands then
        local bList = {"160m","80m","40m","20m","15m","10m"}
        local bw = (rBands.w - (#bList-1)*4) / #bList
        for i, b in ipairs(bList) do
            local active = Model.rx.selectedBand:get() == b and {"state.Active"} or {}
            self.bandButtons[b]:draw(id .. "-b-" .. b, rBands.x + (i-1)*(bw+4), rBands.y, bw, rBands.h, active, lwc)
        end
    end

    local rVol = regions["id-rx-control-vol"]
    if rVol then self.volumeSlider:draw(id .. "-vol", rVol.x, rVol.y, rVol.w, -60, 0, Model.rx.volume.DB:peek(), lwc) end

    local rGain = regions["id-rx-control-gain"]
    if rGain then self.rfGainSlider:draw(id .. "-gain", rGain.x, rGain.y, rGain.w, -20, 60, Model.rx.RF.gainDB:peek(), lwc) end
end

return ReceiverControl
