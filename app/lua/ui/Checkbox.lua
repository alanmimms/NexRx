--[[
  Checkbox Widget
  Modular UI component for boolean toggles.
  Driven by SetBox rules and reactive self.props.
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
        fontSize = 20,
    }
}

function Checkbox:init(def)
    def = def or {}
    Widget.init(self, def)
    self.getText = def.getText
    self.onToggle = def.onToggle
    self.valueObs = def.valueObs
    self.isDown = false
end

function Checkbox:isChecked(overrideChecked)
    if overrideChecked ~= nil then return not not overrideChecked end
    if self.valueObs then return not not self.valueObs:get() end
    return not not self.props.checked
end

function Checkbox:resolveLabel()
    if self.getText then
        return tostring(self.getText() or "")
    end
    return tostring(self.props.label or self.props.text or "Checkbox")
end

function Checkbox:calcMetrics()
    local boxSize = self.props.boxSize or 18
    local spacing = self.props.spacing or 8
    local fontSize = self.props.fontSize or 20
    local label = self:resolveLabel()
    local labelW = TextMixin.measure(label, fontSize)

    if (self.props.prefW or 0) == 0 then
        self.props.prefW = boxSize + spacing + labelW
    end
    if (self.props.prefH or 0) == 0 then
        self.props.prefH = math.max(boxSize, fontSize)
    end
end

function Checkbox:onEvent(event)
    if event.type == "mouseButton" and event.button == "LEFT" then
        if event.isDown then
            self.isDown = true
            state.setActive(self.id)
            if self.lwc then
                self.lwc:addTag("state.Pressed")
                self.lwc:addTag("state.Active")
            end
            return true
        else
            local wasDown = self.isDown or state.isActive(self.id)
            self.isDown = false
            state.setActive(nil)
            if self.lwc then
                self.lwc:removeTag("state.Pressed")
                self.lwc:removeTag("state.Active")
            end
            if wasDown and self:contains(event.x or 0, event.y or 0) then
                local newVal = not self:isChecked()
                self.props.checked = newVal
                if self.lwc then
                    self.lwc:setTag("state.Checked", newVal)
                end
                if self.valueObs and self.valueObs.set then
                    self.valueObs:set(newVal)
                end
                if self.onToggle then
                    self.onToggle(newVal)
                end
            end
            return true
        end
    end
    return Widget.onEvent(self, event)
end

function Checkbox:drawSelf(checked)
    local h = self.props.h or 0
    local isChecked = self:isChecked(checked)

    if self.lwc then
        self.lwc:setTag("state.Checked", isChecked)
    end

    local boxSize = self.props.boxSize or 18
    local spacing = self.props.spacing or 8
    local fontSize = self.props.fontSize or 20
    local label = self:resolveLabel()

    local bgR, bgG, bgB, bgA = state.hexToRgb(self.props.background or "#1e293b")
    local bR, bG, bB, bA = state.hexToRgb(self.props.border or "#475569")
    local aR, aG, aB, aA = state.hexToRgb(self.props.accent or "#3b82f6")
    local bWidth = self.props.borderWidth or 1
    local alpha = self.props.opacity or 1.0

    local bx, by = 0, (h - boxSize) / 2
    System.drawRoundedRect(bx, by, boxSize, boxSize, 3, {bgR, bgG, bgB, (bgA or 1.0) * alpha})
    if bWidth > 0 then
        System.drawRectLines(bx, by, boxSize, boxSize, bWidth, {bR, bG, bB, (bA or 1.0) * alpha})
    end

    if isChecked then
        System.drawLine(bx + 4, by + boxSize / 2, bx + boxSize / 2 - 1, by + boxSize - 5, 2, {aR, aG, aB, (aA or 1.0) * alpha})
        System.drawLine(bx + boxSize / 2 - 1, by + boxSize - 5, bx + boxSize - 4, by + 4, 2, {aR, aG, aB, (aA or 1.0) * alpha})
    end

    TextMixin.draw(boxSize + spacing, (h - fontSize) / 2, label, self, fontSize)
end

return Checkbox
