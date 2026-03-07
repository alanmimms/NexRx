--[[
  Slider Widget
  Modular UI component for numeric value adjustment.
  Behavior and style driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")

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
        borderWidth = 1,
        height = 8,
        handleRadius = 8,
        opacity = 1.0,
    }
}

function Slider.new(options)
    local self = setmetatable({}, Slider)
    self.onAdjust = options and options.onAdjust
    return self
end

local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

function Slider:draw(id, x, y, w, minVal, maxVal, value, parentLWC)
    local tags = {"widget.Slider", "id." .. id}
    
    -- Interaction tags
    if state.isActive(id) then table.insert(tags, "state.Active")
    elseif state.isHot(id) then table.insert(tags, "state.Hovered") end
    
    local lwc = setbox.newContext(tags, parentLWC)
    
    -- Resolved properties
    local h = lwc:getNumber("height")
    local handleR = lwc:getNumber("handleRadius")
    
    state.registerWidget(id, {x=x - handleR, y=y - 4, w=w + handleR*2, h=h + 8}, tags)
    
    -- Hit testing
    if state.pointInRect(state.mouseX, state.mouseY, x - handleR, y - 4, w + handleR*2, h + 8) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end
    
    -- Styling resolution
    local bgR, bgG, bgB = require("ui.Widgets").hexToRgb(lwc:getString("background"))
    local aR, aG, aB = require("ui.Widgets").hexToRgb(lwc:getString("accent"))
    local bR, bG, bB = require("ui.Widgets").hexToRgb(lwc:getString("border"))
    local bWidth = lwc:getNumber("borderWidth")
    local alpha = lwc:getNumber("opacity")
    
    local t = clamp((value - minVal) / (maxVal - minVal), 0, 1)
    
    -- Draw track
    drawRoundedRect(x, y, w, h, h/2, bgR, bgG, bgB, alpha)
    -- Draw fill
    if t > 0 then
        drawRoundedRect(x, y, w * t, h, h/2, aR, aG, aB, alpha)
    end
    -- Draw handle
    local hX = x + w * t
    local hY = y + h/2
    drawCircle(hX, hY, handleR, 1, 1, 1, alpha)
    if bWidth > 0 then
        drawCircleOutline(hX, hY, handleR, bR, bG, bB, alpha, bWidth)
    end
    
    if state.isActive(id) and state.mouseDown then
        local nt = clamp((state.mouseX - x) / w, 0, 1)
        local newValue = minVal + nt * (maxVal - minVal)
        if newValue ~= value and self.onAdjust then self.onAdjust(newValue) end
        return newValue
    end
    
    return value
end

return Slider
