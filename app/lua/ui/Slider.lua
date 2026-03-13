--[[
  Slider Widget
  Modular UI component for numeric value adjustment.
  Behavior and style driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Model = require("Model")
local TextMixin = require("ui.TextMixin")

local Slider = {}
Slider.__index = Slider

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

function Slider.new(options)
    local self = setmetatable({}, Slider)
    self.valueObs = options and options.valueObs
    self.propertyName = options and options.propertyName
    if options and options.getText then self.getText = options.getText end
    return self
end

local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

function Slider:draw(id, x, y, w, h, parentLWC, minVal, maxVal, value)
    local tags = {"widget.Slider", "id." .. id}
    
    -- Interaction tags
    if state.isActive(id) then table.insert(tags, "state.Active")
    elseif state.isHot(id) then table.insert(tags, "state.Hovered") end
    
    local lwc = setbox.newContext(tags, parentLWC)
    
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
    
    -- Total required height
    local totalMinH = trackH + labelH
    local actualH = h or totalMinH
    
    -- Use provided value or current value from observable
    local currentValue = value
    if self.valueObs then currentValue = self.valueObs:get() end
    if currentValue == nil then currentValue = minVal or 0 end

    -- Register with bounds and metadata for event handling (using the full rect including handle overflow)
    state.registerWidget(id, {x=x - handleR, y=y, w=w + handleR*2, h=actualH}, tags, {
        min = minVal,
        max = maxVal,
        property = self.propertyName,
        value = currentValue
    })
    
    -- Hit testing
    if state.pointInRect(state.mouseX, state.mouseY, x - handleR, y, w + handleR*2, actualH) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end
    
    -- Styling resolution
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#1e293b"))
    local aR, aG, aB = state.hexToRgb(lwc:optString("accent", "#3b82f6"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#475569"))
    local bWidth = lwc:optNumber("borderWidth", 1)
    local alpha = lwc:optNumber("opacity", 1.0)

    -- 1. Draw Label at the very top
    if label ~= "" then
        TextMixin.draw(x, y, label, lwc)
    end
    
    -- 2. Draw Track (vertically centered in the remaining space below label)
    local remainingH = actualH - labelH
    local trackY = y + labelH + (remainingH - trackH)/2
    
    local t = clamp((currentValue - minVal) / (maxVal - minVal), 0, 1)
    
    -- Draw track
    drawRoundedRect(x, trackY, w, trackH, trackH/2, bgR, bgG, bgB, alpha)
    -- Draw fill
    if t > 0 then
        drawRoundedRect(x, trackY, w * t, trackH, trackH/2, aR, aG, aB, alpha)
    end
    -- Draw handle
    local hX = x + w * t
    local hY = trackY + trackH/2
    drawCircle(hX, hY, handleR, 1, 1, 1, alpha)
    if bWidth > 0 then
        drawCircleOutline(hX, hY, handleR, bR, bG, bB, alpha, bWidth)
    end
    
    -- Dragging logic
    if state.isActive(id) and state.mouseDown then
        local nt = clamp((state.mouseX - x) / w, 0, 1)
        local newValue = minVal + nt * (maxVal - minVal)
        if newValue ~= currentValue then
            if self.propertyName then
                Model.set(self.propertyName, newValue)
            elseif self.valueObs and self.valueObs.set then
                self.valueObs:set(newValue)
            end
        end
        return newValue
    end
    
    return currentValue
end

return Slider
