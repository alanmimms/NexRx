--[[
  Label Widget
  Modular UI component for text display.
  Driven by reactive self.props and SetBox rules.
]]

local setbox = require("SetBox")
local Stick = require("ui.Stick")
local TextMixin = require("ui.TextMixin")
local Widget = require("ui.Widget")

local Label = Widget.mkType("Label")

-- Default rules for Label widget (very low priority)
setbox.rule {
    id = "label-defaults",
    tags = {"widget.Label"},
    priority = -1000,
    apply = {
        text = "",
        foreground = "#ffffff",
        fontSize = 20,
        opacity = 1.0,
    }
}

function Label:init(def)
    Widget.init(self, def)
    if def and def.getText then
        self.getText = def.getText
    end
end

function Label:resolveText()
    if self.getText then
        return tostring(self.getText(self.lwc) or "")
    end
    local t = self.props.text
    if t ~= nil and t ~= "" then
        return tostring(t)
    end
    local title = self.props.title
    if title ~= nil then
        return tostring(title)
    end
    return tostring(t or "")
end

function Label:calcMetrics()
    local text = self:resolveText()
    local fontSize = self.props.fontSize or 20
    local tw = TextMixin.measure(text, fontSize)
    if (self.props.prefW or 0) == 0 then self.props.prefW = tw end
    if (self.props.prefH or 0) == 0 then self.props.prefH = TextMixin.getLineHeight(fontSize) end
end

function Label:drawSelf()
    Widget.drawSelf(self)
    local w, h = self.props.w or 0, self.props.h or 0
    local text = self:resolveText()
    if text == "" then return end

    local fontSize = self.props.fontSize or 20
    local stick = self.props.stick or (Stick.L | Stick.T)
    local textW = TextMixin.measure(text, fontSize)
    local lineH = TextMixin.getLineHeight(fontSize)

    local tx, ty = 0, (h - lineH) / 2
    if (stick & Stick.R) ~= 0 and (stick & Stick.L) ~= 0 then
        tx = (w - textW) / 2
    elseif (stick & Stick.R) ~= 0 then
        tx = w - textW
    elseif (stick & Stick.L) == 0 then
        tx = (w - textW) / 2
    end

    if (stick & Stick.B) ~= 0 and (stick & Stick.T) == 0 then
        ty = h - lineH
    elseif (stick & Stick.T) ~= 0 and (stick & Stick.B) == 0 then
        ty = 0
    end

    TextMixin.draw(tx, ty, text, self, fontSize)
end

return Label
