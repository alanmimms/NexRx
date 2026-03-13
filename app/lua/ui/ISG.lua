--[[
  ISG Widget Module
  Modular UI component for Internal Signal Generator control.
  Fully rule-driven behavior and style.
]]

local state = require("ui.State")
local Label = require("ui.Label")
local Button = require("ui.Button")
local Checkbox = require("ui.Checkbox")
local Slider = require("ui.Slider")
local FrequencyDisplay = require("ui.FrequencyDisplay")
local layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")

local ISG = {}
ISG.__index = ISG

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
        height = function(lwc)
            local spacing = 4 -- layout.defaultSpacing
            -- 36(freq) + 24(rounding) + 26(toggle) + 24(slider) = 110
            -- newLines: 44, 32, 28, 24 = 128
            -- total: 110 + 128 = 238
            -- plus margins and spacing: 32(top) + 12(pad) + (4*spacing) = 60
            -- Result ~ 298
            return lwc:getNumber("topMargin") + 44+32+28+24 + lwc:getNumber("padding") + (4 * spacing) + 12
        end,
        labelEnabled = "Enabled",
        sliderMin = 0.1e6,
        sliderMax = 30.0e6,
    }
}

function ISG.new(props)
    local self = setmetatable({}, ISG)
    self.ISG = props.ISG -- Model.ISG
    
    -- Initialize widget instances
    self.titleLabel = Label.new()
    self.freqDisplay = FrequencyDisplay.new({
        valueObs = self.ISG.frequencyHz
    })
    self.enableCheckbox = Checkbox.new({
        onToggle = function(val) Model.set("isgEnabled", val) end
    })
    self.freqSlider = Slider.new({
        valueObs = self.ISG.frequencyHz,
        propertyName = "isgFrequency"
    })
    self.round1k = Button.new({
        getText = function() return "<0k>" end,
        onClick = function()
            Model.roundFrequency("isgFrequency", 1000)
        end
    })
    self.round100 = Button.new({
        getText = function() return "<00>" end,
        onClick = function()
            Model.roundFrequency("isgFrequency", 100)
        end
    })
    
    return self
end

function ISG:draw(id, x, y, w, h, parentLWC)
    local lwc = setbox.newContext({"widget.Panel", "widget.ISGFrame", "id." .. id}, parentLWC)
    
    -- Background and border
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
    
    local freq = self.ISG.frequencyHz:get()
    
    -- Frequency Display
    local cx, cy = layout.getCursor()
    self.freqDisplay:draw(id .. "-freq-disp", cx, cy, w - padding * 2, 36, lwc, _G.isgFreqEntryText or "", _G.isgFreqEntryCursor, {"IsgControl"})
    layout.newLine(44)

    -- Rounding buttons
    layout.beginHorizontal(0)
    local rbw = (w - padding * 2 - 4) / 2
    self.round1k:draw(id .. "-round-1k", cx, layout.getCursorY(), rbw, 24, lwc)
    layout.space(4)
    self.round100:draw(id .. "-round-100", cx + rbw + 4, layout.getCursorY(), rbw, 24, lwc)
    layout.endHorizontal(); layout.newLine(32)

    -- Enabled Toggle
    cx, cy = layout.getCursor()
    -- Set checkbox label from rule
    self.enableCheckbox.getText = function() return lwc:getString("labelEnabled") end
    self.enableCheckbox:draw(id .. "-en", cx, cy, w - padding * 2, 28, lwc, self.ISG.enabled:get())
    layout.newLine(28)

    -- Frequency Slider
    cx, cy = layout.getCursor()
    local sMin = lwc:getNumber("sliderMin")
    local sMax = lwc:getNumber("sliderMax")
    self.freqSlider:draw(id .. "-fr-slider", cx, cy, w - padding * 2, 24, lwc, sMin, sMax, freq)
    layout.newLine(24)
    
    layout.endRegion()
end

return ISG
