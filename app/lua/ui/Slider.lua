--[[
  Slider Widget
  Modular UI component for numeric value adjustment.
  Behavior and style driven by SetBox rules and reactive self.props.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Model = require("Model")
local TextMixin = require("ui.TextMixin")
local Widget = require("ui.Widget")

local Slider = Widget.mkType("Slider")

-- Default rules for Slider widget (very low priority)
setbox.rule {
    id = "slider-defaults",
    tags = {"widget.Slider"},
    priority = -1000,
    apply = {
        background = "#1e293b",
        accent = "#3b82f6",
        border = "#475569",
        foreground = "#ffffff",
        borderWidth = 1,
        trackHeight = 8,
        handleRadius = 8,
        opacity = 1.0,
        labelSpacing = 4,
        fontSize = 20,
    }
}

function Slider:init(def)
    def = def or {}
    Widget.init(self, def)
    self.valueObs = def.valueObs
    self.propertyName = def.propertyName
    self.getText = def.getText
    self.minVal = def.minVal or 0
    self.maxVal = def.maxVal or 100
    self.onChanged = def.onChanged
end

local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

function Slider:getCurrentValue()
    if self.valueObs then
        return self.valueObs:get()
    elseif self.propertyName and Model.has(self.propertyName) then
        return Model.getNumber(self.propertyName)
    elseif self.props.value ~= nil then
        return self.props.value
    end
    return self.minVal or 0
end

function Slider:setCurrentValue(newVal)
    self.props.value = newVal
    if self.valueObs and self.valueObs.set then
        self.valueObs:set(newVal)
    end
    if self.propertyName then
        Model.set(self.propertyName, newVal)
    end
    if self.onChanged then
        self.onChanged(newVal)
    end
end

function Slider:resolveLabel()
    if self.getText then
        return tostring(self.getText() or "")
    end
    local lbl = self.props.label or self.props.text
    if lbl ~= nil then
        return tostring(lbl)
    end
    return ""
end

function Slider:calcMetrics()
    local trackH = self.props.trackHeight or 8
    local labelSpacing = self.props.labelSpacing or 4
    local fontSize = self.props.fontSize or 20
    local hasLabel = (self.getText ~= nil) or (self.props.label ~= nil) or (self.props.text ~= nil and self.props.text ~= "")
    local labelH = hasLabel and (TextMixin.getLineHeight(fontSize) + labelSpacing) or 0

    if (self.props.prefW or 0) == 0 then self.props.prefW = 100 end
    if (self.props.prefH or 0) == 0 then self.props.prefH = trackH + labelH end
end

function Slider:onEvent(event)
    local minVal = self.minVal or 0
    local maxVal = self.maxVal or 100
    local range = maxVal - minVal
    local current = self:getCurrentValue()
    local w = self.props.w or 1

    if event.type == "mouseWheel" or event.type == "key" then
        local delta = 0
        local stepFraction = 0.01
        if isCtrlDown and isCtrlDown() then stepFraction = 0.001
        elseif isShiftDown and isShiftDown() then stepFraction = 0.1 end

        local step = range * stepFraction

        if event.type == "mouseWheel" then
            delta = (event.delta or 0) * step
        elseif event.isDown then
            if event.key == "RIGHT" or event.key == "UP" then delta = step
            elseif event.key == "LEFT" or event.key == "DOWN" then delta = -step end
        end

        if delta ~= 0 then
            local newVal = clamp(current + delta, minVal, maxVal)
            self:setCurrentValue(newVal)
            return true
        end
    end

    if event.type == "mouseButton" and event.button == "LEFT" then
        if event.isDown then
            state.setActive(self.id)
            if self.lwc then
                self.lwc:addTag("state.Active")
                self.lwc:addTag("state.Pressed")
            end
            local nt = clamp((event.x or 0) / math.max(1, w), 0, 1)
            local newValue = minVal + nt * range
            self:setCurrentValue(newValue)
            return true
        else
            state.setActive(nil)
            if self.lwc then
                self.lwc:removeTag("state.Active")
                self.lwc:removeTag("state.Pressed")
            end
            return true
        end
    elseif event.type == "mouseMotion" and state.isActive(self.id) then
        local nt = clamp((event.x or 0) / math.max(1, w), 0, 1)
        local newValue = minVal + nt * range
        self:setCurrentValue(newValue)
        return true
    end

    return Widget.onEvent(self, event)
end

function Slider:drawSelf()
    local w, h = self.props.w or 0, self.props.h or 0

    local trackH = self.props.trackHeight or 8
    local handleR = self.props.handleRadius or 8
    local labelSpacing = self.props.labelSpacing or 4
    local fontSize = self.props.fontSize or 20

    local label = self:resolveLabel()
    local labelH = 0
    if label ~= "" then
        labelH = TextMixin.getLineHeight(fontSize) + labelSpacing
    end

    local currentValue = self:getCurrentValue()
    local minVal = self.minVal or 0
    local maxVal = self.maxVal or 100

    local bgR, bgG, bgB, bgA = state.hexToRgb(self.props.background or "#1e293b")
    local aR, aG, aB, aA = state.hexToRgb(self.props.accent or "#3b82f6")
    local bR, bG, bB, bA = state.hexToRgb(self.props.border or "#475569")
    local bWidth = self.props.borderWidth or 1
    local alpha = self.props.opacity or 1.0

    if label ~= "" then
        TextMixin.draw(0, 0, label, self, fontSize)
    end

    local remainingH = h - labelH
    local trackY = labelH + (remainingH - trackH) / 2
    local denom = (maxVal - minVal)
    local t = denom ~= 0 and clamp((currentValue - minVal) / denom, 0, 1) or 0

    System.drawRoundedRect(0, trackY, w, trackH, trackH / 2, {bgR, bgG, bgB, (bgA or 1.0) * alpha})
    if t > 0 then
        System.drawRoundedRect(0, trackY, w * t, trackH, trackH / 2, {aR, aG, aB, (aA or 1.0) * alpha})
    end

    local hX = w * t
    local hY = trackY + trackH / 2
    System.drawCircle(hX, hY, handleR, {1, 1, 1, alpha})
    if bWidth > 0 then
        System.drawCircleOutline(hX, hY, handleR, bWidth, {bR, bG, bB, (bA or 1.0) * alpha})
    end
end

-- =============================================================================
-- DiscreteSlider Widget
-- Handles categorical choices (like Modes or Bands)
-- =============================================================================

local DiscreteSlider = Widget.mkType("DiscreteSlider")
Slider.DiscreteSlider = DiscreteSlider

setbox.rule {
    id = "discrete-slider-defaults",
    tags = {"widget.DiscreteSlider"},
    priority = -1000,
    apply = {
        background = "#1e293b",
        accent = "#3b82f6",
        border = "#475569",
        opacity = 1.0,
    }
}

function DiscreteSlider:init(def)
    def = def or {}
    Widget.init(self, def)
    self.stops = def.stops or {}
    self.valueObs = def.valueObs
    self.propertyName = def.propertyName
    self.onChanged = def.onChanged
end

function DiscreteSlider:getCurrentValue()
    if self.valueObs then
        return self.valueObs:get()
    elseif self.propertyName and Model.has(self.propertyName) then
        return Model.get(self.propertyName)
    end
    return self.props.value
end

function DiscreteSlider:setCurrentValue(val)
    self.props.value = val
    if self.valueObs and self.valueObs.set then
        self.valueObs:set(val)
    end
    if self.propertyName then
        Model.set(self.propertyName, val)
    end
    if self.onChanged then
        self.onChanged(val)
    end
end

function DiscreteSlider:calcMetrics()
    if (self.props.prefW or 0) == 0 then self.props.prefW = 200 end
    if (self.props.prefH or 0) == 0 then self.props.prefH = 30 end
end

function DiscreteSlider:onEvent(event)
    local nStops = #self.stops
    if nStops == 0 then return false end

    local w = math.max(1, self.props.w or 1)
    local currentValue = self:getCurrentValue()

    if event.type == "mouseButton" and event.button == "LEFT" then
        if event.isDown then
            state.setActive(self.id)
            if self.lwc then
                self.lwc:addTag("state.Active")
                self.lwc:addTag("state.Pressed")
            end
            local nt = math.max(0, math.min(0.999, (event.x or 0) / w))
            local stopIdx = math.floor(nt * nStops) + 1
            local stop = self.stops[stopIdx]
            if stop and stop.value ~= currentValue then
                self:setCurrentValue(stop.value)
            end
            return true
        else
            state.setActive(nil)
            if self.lwc then
                self.lwc:removeTag("state.Active")
                self.lwc:removeTag("state.Pressed")
            end
            return true
        end
    elseif event.type == "mouseMotion" and state.isActive(self.id) then
        local nt = math.max(0, math.min(0.999, (event.x or 0) / w))
        local stopIdx = math.floor(nt * nStops) + 1
        local stop = self.stops[stopIdx]
        if stop and stop.value ~= currentValue then
            self:setCurrentValue(stop.value)
        end
        return true
    end

    return Widget.onEvent(self, event)
end

function DiscreteSlider:drawSelf()
    local w, h = self.props.w or 0, self.props.h or 0
    local currentValue = self:getCurrentValue()
    local nStops = #self.stops

    local bR, bG, bB, bA = state.hexToRgb(self.props.border or "#475569")
    local alpha = self.props.opacity or 1.0
    System.drawRectLines(0, 0, w, h, 1, {bR, bG, bB, (bA or 1.0) * alpha})

    if nStops == 0 then return end

    local stopW = w / nStops
    local bgR, bgG, bgB, bgA = state.hexToRgb(self.props.background or "#1e293b")
    local aR, aG, aB, aA = state.hexToRgb(self.props.accent or "#3b82f6")

    for i, stop in ipairs(self.stops) do
        local sx = (i - 1) * stopW
        local isSelected = (stop.value == currentValue)

        local r, g, b, a = bgR, bgG, bgB, bgA
        if isSelected then r, g, b, a = aR, aG, aB, aA end

        System.drawRect(sx + 1, 1, stopW - 2, h - 2, {r, g, b, (a or 1.0) * alpha})
        System.drawRectLines(sx + 1, 1, stopW - 2, h - 2, 1, {1, 1, 1, 0.5 * alpha})

        local label = tostring(stop.label)
        local tw = System.measureText(label, 16)
        System.drawText(label, sx + (stopW - tw) / 2, (h - 16) / 2, 16, {1, 1, 1, alpha})
    end
end

return Slider
