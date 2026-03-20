--[[
  Panel Widget
  Modular UI component for containers and backgrounds.
  Style and behavior driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")

local Panel = Widget.mkType("Panel")

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
    Widget.init(self, def)
end

function Panel:calcMetrics()
    if self.metrics.prefW == 0 then self.metrics.prefW = 10 end
    if self.metrics.prefH == 0 then self.metrics.prefH = 10 end
end

function Panel:drawSelf()
    local w, h = self.props.w, self.props.h
    local lwc = self.lwc
    
    -- Style resolution from rules
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#0f172a"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#1e293b"))
    local bWidth = lwc:optNumber("borderWidth", 0)
    local radius = lwc:optNumber("borderRadius", 0)
    local alpha = lwc:optNumber("opacity", 1.0)
    
    -- Draw at local 0,0
    System.drawRoundedRect(0, 0, w, h, radius, {bgR, bgG, bgB, alpha})
    if bWidth > 0 then
        System.drawRectLines(0, 0, w, h, bWidth, {bR, bG, bB, alpha})
    end
end

return Panel
