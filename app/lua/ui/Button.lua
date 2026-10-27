--[[
  Button Widget
  Modular UI component for clickable actions.
  Behavior and style driven by SetBox rules and reactive self.props.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local TextMixin = require("ui.TextMixin")
local Widget = require("ui.Widget")

local Button = Widget.mkType("Button")

-- Default rules for Button widget (very low priority)
setbox.rule {
    id = "button-defaults",
    tags = {"widget.Button"},
    priority = -1000,
    apply = {
        background = "#3b82f6",
        foreground = "#ffffff",
        border = "#2563eb",
        borderWidth = 1,
        borderRadius = 6,
        opacity = 1.0,
        padding = 8,
        width = 100,
        height = 32,
        fontSize = 20,
    }
}

function Button:init(def)
    def = def or {}
    Widget.init(self, def)
    self.getText = def.getText
    self.onClick = def.onClick or def.onClicked
    self.onClicked = self.onClick
    self.isDown = false
end

function Button:resolveLabel()
    if self.getText then
        return tostring(self.getText() or "")
    end
    local lbl = self.props.label or self.props.text
    if lbl ~= nil then
        return tostring(lbl)
    end
    return tostring(self.id or "Button")
end

function Button:calcMetrics()
    if (self.props.prefW or 0) == 0 then
        self.props.prefW = self.props.width or 100
    end
    if (self.props.prefH or 0) == 0 then
        self.props.prefH = self.props.height or 32
    end
end

function Button:onEvent(event)
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
                local cb = self.onClick or self.onClicked
                if cb then cb(self) end
            end
            return true
        end
    end
    return Widget.onEvent(self, event)
end

function Button:drawSelf()
    local w, h = self.props.w or 0, self.props.h or 0

    local selected = self.props.selected or self.selected
    if self.lwc then
        self.lwc:setTag("state.Selected", not not selected)
    end

    local bg = self.props.background or self.props.backgroundColor or "#3b82f6"
    local fg = self.props.foreground or self.props.textColor or "#ffffff"
    if selected and not self.lwc:has("background") then
        bg = "#facc15"
        fg = "#000000"
    end

    local bgR, bgG, bgB, bgA = state.hexToRgb(bg)
    local fgR, fgG, fgB, fgA = state.hexToRgb(fg)
    local bR, bG, bB, bA = state.hexToRgb(self.props.border or self.props.borderColor or "#2563eb")
    local bWidth = self.props.borderWidth or 1
    local radius = self.props.borderRadius or 6
    local alpha = self.props.opacity or 1.0
    local fontSize = self.props.fontSize or 20

    System.drawRoundedRect(0, 0, w, h, radius, {bgR, bgG, bgB, (bgA or 1.0) * alpha})
    if bWidth > 0 then
        System.drawRectLines(0, 0, w, h, bWidth, {bR, bG, bB, (bA or 1.0) * alpha})
    end

    local label = self:resolveLabel()
    local labelW = TextMixin.measure(label, fontSize)
    local lineH = TextMixin.getLineHeight(fontSize)
    System.drawText(label, (w - labelW) / 2, (h - lineH) / 2, fontSize, {fgR, fgG, fgB, (fgA or 1.0) * alpha})
end

return Button
