--[[
  Panel Widget
  Modular UI component for containers and backgrounds.
  Style and behavior driven entirely by SetBox rules and reactive self.props.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")

local Panel = Widget.mkType("Panel", Widget.Container)

-- Default rules for Panel widget (very low priority)
setbox.rule {
    id = "panel-defaults",
    tags = {"widget.Panel"},
    priority = -1000,
    apply = {
        background = "#0f172a",
        border = "#1e293b",
        borderWidth = 0,
        borderRadius = 0,
        opacity = 1.0,
    }
}

function Panel:init(def)
    Widget.Container.init(self, def)
end

function Panel:calcMetrics()
    Widget.Container.calcMetrics(self)
end

function Panel:drawSelf()
    local w, h = self.props.w or 0, self.props.h or 0

    local bg = self.props.background or self.props.backgroundColor or "#0f172a"
    local border = self.props.border or self.props.borderColor or "#1e293b"
    local bWidth = self.props.borderWidth or 0
    local radius = self.props.borderRadius or 0
    local alpha = self.props.opacity or 1.0

    local bgR, bgG, bgB, bgA = state.hexToRgb(bg)
    local bR, bG, bB, bA = state.hexToRgb(border)

    System.drawRoundedRect(0, 0, w, h, radius, {bgR, bgG, bgB, (bgA or 1.0) * alpha})
    if bWidth > 0 then
        System.drawRectLines(0, 0, w, h, bWidth, {bR, bG, bB, (bA or 1.0) * alpha})
    end
end

return Panel
