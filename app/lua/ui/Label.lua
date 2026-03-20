--[[
  Label Widget
  Modular UI component for text display.
  Everything comes from SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local TextMixin = require("ui.TextMixin")

local Widget = require("ui.Widget")
local Label = Widget.mkType("Label")

-- Default rules for Label widget (very low priority)
setbox.rule {
    id = "label-defaults",
    tags = {"widget.Label"},
    priority = -1000,
    apply = {
        text = "", -- Default empty text
        foreground = "#ffffff",
        opacity = 1.0,
    }
}

function Label:init(def)
    Widget.init(self, def)
    if def and def.getText then
        self.getText = def.getText
    end
end

function Label:calcMetrics()
    local lwc = self.lwc
    local text = ""
    if self.getText then
        text = self.getText(lwc)
    elseif lwc:has("text") then
        text = lwc:getString("text")
    else
        text = lwc:has("title") and lwc:getString("title") or ""
    end
    local tw = TextMixin.measure(text)
    if self.metrics.prefW == 0 then self.metrics.prefW = tw end
    if self.metrics.prefH == 0 then self.metrics.prefH = TextMixin.getLineHeight() end
end

function Label:drawSelf()
    local id, w, h = self.id, self.props.w, self.props.h
    local lwc = self.lwc
    
    local text = ""
    if self.getText then
        text = self.getText(lwc)
    elseif lwc:has("text") then
        text = lwc:getString("text")
    else
        -- Fallback to title property if no text property
        text = lwc:has("title") and lwc:getString("title") or ""
    end
    
    TextMixin.draw(0, 0, text, lwc)
end

return Label
