--[[
  Slider Widget
  Modular UI component for numeric value adjustment.
  Behavior and style driven entirely by SetBox rules.
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
        foreground = "#ffffff", -- Default label color
        borderWidth = 1,
        trackHeight = 8,
        handleRadius = 8,
        opacity = 1.0,
        labelSpacing = 4, -- Spacing between label and slider track
    }
}

function Slider:init(def)
    Widget.init(self, def)
    self.valueObs = def.valueObs
    self.propertyName = def.propertyName
    self.getText = def.getText
    self.minVal = def.minVal or 0
    self.maxVal = def.maxVal or 100
end

local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

function Slider:calcMetrics()
    local trackH = self.lwc:optNumber("trackHeight", 8)
    local labelSpacing = self.lwc:optNumber("labelSpacing", 4)
    local labelH = (self.getText or self.lwc:has("label") or self.lwc:has("text")) and (TextMixin.getLineHeight() + labelSpacing) or 0
    
    if self.metrics.prefW == 0 then self.metrics.prefW = 100 end
    if self.metrics.prefH == 0 then self.metrics.prefH = trackH + labelH end
end

function Slider:handleEvent(event)
    local minVal = self.minVal or 0
    local maxVal = self.maxVal or 100
    local range = maxVal - minVal
    local current = self.valueObs and self.valueObs:get() or minVal

    if event.type == "mouseWheel" or event.type == "key" then
        local delta = 0
        local stepFraction = 0.01
        if isCtrlDown and isCtrlDown() then stepFraction = 0.001
        elseif isShiftDown and isShiftDown() then stepFraction = 0.1 end
        
        local step = range * stepFraction

        if event.type == "mouseWheel" then
            delta = event.delta * step
        elseif event.isDown then
            if event.key == "RIGHT" or event.key == "UP" then delta = step
            elseif event.key == "LEFT" or event.key == "DOWN" then delta = -step end
        end

        if delta ~= 0 then
            local newVal = math.max(minVal, math.min(maxVal, current + delta))
            if self.valueObs and self.valueObs.set then self.valueObs:set(newVal) end
            return true
        end
    end

    if event.type == "mouseButton" and event.button == "LEFT" then
        if event.isDown then
            state.setActive(self.id)
            return true
        end
    end
    
    return Widget.handleEvent(self, event)
end

function Slider:drawSelf()
    local id, w, h = self.id, self.props.w, self.props.h
    local lwc = self.lwc
    
    -- Resolved properties
    local trackH = lwc:optNumber("trackHeight", 8)
    local handleR = lwc:optNumber("handleRadius", 8)
    local labelSpacing = lwc:optNumber("labelSpacing", 4)
    
    -- Label text from callback or rule
    local label = ""
    if self.getText then
        label = self.getText()
    elseif lwc:has("label") then
        label = lwc:getString("label")
    elseif lwc:has("text") then
        label = lwc:getString("text")
    end

    local labelH = 0
    if label ~= "" then
        labelH = TextMixin.getLineHeight() + labelSpacing
    end
    
    -- Use current value from observable or fallback
    local currentValue = self.props.value
    if self.valueObs then currentValue = self.valueObs:get() end
    
    local minVal = self.minVal or 0
    local maxVal = self.maxVal or 100
    if currentValue == nil then currentValue = minVal end

    -- Styling resolution
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#1e293b"))
    local aR, aG, aB = state.hexToRgb(lwc:optString("accent", "#3b82f6"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#475569"))
    local bWidth = lwc:optNumber("borderWidth", 1)
    local alpha = lwc:optNumber("opacity", 1.0)

    -- 1. Draw Label at the very top (Local 0,0)
    if label ~= "" then
        TextMixin.draw(0, 0, label, lwc)
    end
    
    -- 2. Draw Track (vertically centered in the remaining space below label)
    local remainingH = h - labelH
    local trackY = labelH + (remainingH - trackH)/2
    
    local t = clamp((currentValue - minVal) / (maxVal - minVal), 0, 1)
    
    -- Draw track
    System.drawRoundedRect(0, trackY, w, trackH, trackH/2, {bgR, bgG, bgB, alpha})
    -- Draw fill
    if t > 0 then
        System.drawRoundedRect(0, trackY, w * t, trackH, trackH/2, {aR, aG, aB, alpha})
    end
    -- Draw handle
    local hX = w * t
    local hY = trackY + trackH/2
    System.drawCircle(hX, hY, handleR, {1, 1, 1, alpha})
    if bWidth > 0 then
        System.drawCircleOutline(hX, hY, handleR, bWidth, {bR, bG, bB, alpha})
    end
end

-- =============================================================================
-- DiscreteSlider Widget
-- Handles categorical choices (like Modes or Bands)
-- =============================================================================

local DiscreteSlider = Widget.mkType("DiscreteSlider")
Slider.DiscreteSlider = DiscreteSlider

function DiscreteSlider:init(def)
    Widget.init(self, def)
    self.stops = def.stops or {} -- { {label="USB", value="USB"}, ... }
    self.valueObs = def.valueObs
    self.propertyName = def.propertyName
    self.onChanged = def.onChanged
end

function DiscreteSlider:calcMetrics()
    if self.metrics.prefW == 0 then self.metrics.prefW = 200 end
    if self.metrics.prefH == 0 then self.metrics.prefH = 30 end
end

function DiscreteSlider:handleEvent(event)
    local nStops = #self.stops
    if nStops == 0 then return false end
    
    local w = self.props.w
    local currentValue = self.valueObs and self.valueObs:get() or nil

    if event.type == "mouseButton" and event.button == "LEFT" then
        if event.isDown then
            state.setActive(self.id)
            -- Immediate update on click
            local nt = math.max(0, math.min(0.999, event.x / w))
            local stopIdx = math.floor(nt * nStops) + 1
            local stop = self.stops[stopIdx]
            if stop and stop.value ~= currentValue then
                if self.valueObs and self.valueObs.set then self.valueObs:set(stop.value) end
                if self.onChanged then self.onChanged(stop.value) end
            end
            return true
        end
    elseif event.type == "mouseMotion" and state.isActive(self.id) then
        local nt = math.max(0, math.min(0.999, event.x / w))
        local stopIdx = math.floor(nt * nStops) + 1
        local stop = self.stops[stopIdx]
        if stop and stop.value ~= currentValue then
            if self.valueObs and self.valueObs.set then self.valueObs:set(stop.value) end
            if self.onChanged then self.onChanged(stop.value) end
        end
        return true
    end
    
    return Widget.handleEvent(self, event)
end

function DiscreteSlider:drawSelf()
    local id, w, h = self.id, self.props.w, self.props.h
    local lwc = self.lwc
    
    local currentValue = self.valueObs and self.valueObs:get() or nil
    local nStops = #self.stops

    -- Draw outer border
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#475569"))
    System.drawRectLines(0, 0, w, h, 1, {bR, bG, bB, 1.0})

    if nStops == 0 then return end
    
    local stopW = w / nStops
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#1e293b"))
    local aR, aG, aB = state.hexToRgb(lwc:optString("accent", "#3b82f6"))
    
    for i, stop in ipairs(self.stops) do
        local sx = (i-1) * stopW
        local isSelected = (stop.value == currentValue)
        
        local r, g, b = bgR, bgG, bgB
        if isSelected then r, g, b = aR, aG, aB end
        
        System.drawRect(sx + 1, 1, stopW - 2, h - 2, {r, g, b, 1.0})
        System.drawRectLines(sx + 1, 1, stopW - 2, h - 2, 1, {1, 1, 1, 0.5})
        
        local label = tostring(stop.label)
        local tw = System.measureText(label, 16)
        System.drawText(label, sx + (stopW - tw)/2, (h - 16)/2, 16, {1, 1, 1, 1})
    end
end

return Slider
