--[[
  Checkbox Widget
  Modular UI component for boolean toggles.
  Fully rule-driven via SetBox.
]]

local setbox = require("setbox")
local state = require("ui.state")

local Checkbox = {}
Checkbox.__index = Checkbox

-- Default rules for Checkbox widget (very low priority)
setbox.rule {
    id = "checkbox-defaults",
    tags = {"widget.Checkbox"},
    priority = -1000,
    apply = {
        background = "#1e293b",
        foreground = "#ffffff",
        border = "#475569",
        accent = "#3b82f6",
        borderWidth = 1,
        boxSize = 18,
        spacing = 8,
        opacity = 1.0,
        label = "Checkbox",
    }
}

function Checkbox.new(options)
    local self = setmetatable({}, Checkbox)
    if options and options.getText then
        self.getText = options.getText
    end
    self.onToggle = options and options.onToggle
    return self
end

function Checkbox:draw(id, x, y, checked, parentLWC)
    local tags = {"widget.Checkbox", "id." .. id}
    if checked then table.insert(tags, "state.Checked") end
    
    -- Interaction tags
    if state.isActive(id) then table.insert(tags, "state.Pressed")
    elseif state.isHot(id) then table.insert(tags, "state.Hovered") end
    
    local lwc = setbox.newContext(tags, parentLWC)
    
    -- Resolved properties
    local boxSize = lwc:getNumber("boxSize")
    local spacing = lwc:getNumber("spacing")
    
    local label = ""
    if self.getText then
        label = self.getText()
    else
        label = lwc:getString("label")
    end
    
    local labelW = measureText(label)
    local w = boxSize + spacing + labelW
    local h = boxSize
    
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, tags)
    
    -- Hit testing
    if state.pointInRect(state.mouseX, state.mouseY, x, y, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end
    
    -- Styling from rules
    local bgR, bgG, bgB = require("ui.widgets").hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = require("ui.widgets").hexToRgb(lwc:getString("foreground"))
    local bR, bG, bB = require("ui.widgets").hexToRgb(lwc:getString("border"))
    local aR, aG, aB = require("ui.widgets").hexToRgb(lwc:getString("accent"))
    local bWidth = lwc:getNumber("borderWidth")
    local alpha = lwc:getNumber("opacity")
    
    -- Draw box
    drawRoundedRect(x, y, boxSize, boxSize, 3, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, boxSize, boxSize, bR, bG, bB, alpha, bWidth)
    end
    
    if checked then
        -- Draw checkmark
        drawLine(x + 4, y + boxSize/2, x + boxSize/2 - 1, y + boxSize - 5, aR, aG, aB, alpha, 2)
        drawLine(x + boxSize/2 - 1, y + boxSize - 5, x + boxSize - 4, y + 4, aR, aG, aB, alpha, 2)
    end
    
    -- Draw label
    drawText(x + boxSize + spacing, y + (boxSize - getLineHeight()) / 2, label, fgR, fgG, fgB, alpha)
    
    local clicked = state.wasClicked(id)
    if clicked and self.onToggle then self.onToggle(not checked) end
    return clicked
end

return Checkbox
