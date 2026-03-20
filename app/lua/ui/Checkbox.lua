--[[
  Checkbox Widget
  Modular UI component for boolean toggles.
  Fully rule-driven via SetBox.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local TextMixin = require("ui.TextMixin")
local Widget = require("ui.Widget")

local Checkbox = Widget.mkType("Checkbox")

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

function Checkbox:init(def)
    Widget.init(self, def)
    self.getText = def.getText
    self.onToggle = def.onToggle
    self.valueObs = def.valueObs
end

function Checkbox:calcMetrics()
    local boxSize = self.lwc:optNumber("boxSize", 18)
    local spacing = self.lwc:optNumber("spacing", 8)
    local label = self.getText and self.getText() or self.lwc:optString("label", "Checkbox")
    local labelW = System.measureText(label, 20)
    
    if self.metrics.prefW == 0 then self.metrics.prefW = boxSize + spacing + labelW end
    if self.metrics.prefH == 0 then self.metrics.prefH = math.max(boxSize, 20) end
end

function Checkbox:drawSelf(checked)
    local id, w, h = self.id, self.props.w, self.props.h
    
    if self.valueObs then checked = self.valueObs:get() end

    local tags = {"widget.Checkbox", "id." .. id}
    if checked then table.insert(tags, "state.Checked") end
    
    -- Interaction tags
    if state.isActive(id) then table.insert(tags, "state.Pressed")
    elseif state.isHot(id) then table.insert(tags, "state.Hovered") end
    
    local lwc = self.lwc
    
    -- Resolved properties
    local boxSize = lwc:optNumber("boxSize", 18)
    local spacing = lwc:optNumber("spacing", 8)
    
    local label = ""
    if self.getText then
        label = self.getText()
    else
        label = lwc:optString("label", "Checkbox")
    end
    
    state.registerWidget(id, {x=0, y=0, w=w, h=h}, tags)
    
    -- Hit testing (Local 0,0)
    if state.pointInRect(state.mouseX, state.mouseY, 0, 0, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end
    
    -- Styling from rules
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#1e293b"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#475569"))
    local aR, aG, aB = state.hexToRgb(lwc:optString("accent", "#3b82f6"))
    local bWidth = lwc:optNumber("borderWidth", 1)
    local alpha = lwc:optNumber("opacity", 1.0)
    
    -- Draw box
    System.drawRoundedRect(0, (h - boxSize)/2, boxSize, boxSize, 3, {bgR, bgG, bgB, alpha})
    if bWidth > 0 then
        System.drawRectLines(0, (h - boxSize)/2, boxSize, boxSize, bWidth, {bR, bG, bB, alpha})
    end
    
    if checked then
        -- Draw checkmark
        local bx, by = 0, (h - boxSize)/2
        System.drawLine(bx + 4, by + boxSize/2, bx + boxSize/2 - 1, by + boxSize - 5, 2, {aR, aG, aB, alpha})
        System.drawLine(bx + boxSize/2 - 1, by + boxSize - 5, bx + boxSize - 4, by + 4, 2, {aR, aG, aB, alpha})
    end
    
    -- Draw label
    TextMixin.draw(boxSize + spacing, (h - 20) / 2, label, lwc)
    
    if state.wasClicked(id) then
        local newVal = not checked
        if self.onToggle then self.onToggle(newVal) end
        if self.valueObs and self.valueObs.set then self.valueObs:set(newVal) end
    end
end

return Checkbox
