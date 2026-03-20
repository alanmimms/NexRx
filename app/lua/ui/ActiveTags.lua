--[[
  Active Tags Debug Widget
  Modular UI component for displaying current SetBox tags.
  Behavior and style driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")

local ActiveTags = Widget.mkType("ActiveTags")

-- Default rules for ActiveTags widget (very low priority)
setbox.rule {
    id = "active-tags-defaults",
    tags = {"widget.ActiveTagsViewer"},
    priority = -1000,
    apply = {
        background = "#08081a",
        opacity = 0.8,
        borderRadius = 4,
        padding = 8,
        title = "ACTIVE TAGS",
    }
}

function ActiveTags:init(def)
    Widget.init(self, def)
end

function ActiveTags:calcMetrics()
    if self.metrics.prefW == 0 then self.metrics.prefW = 180 end
    if self.metrics.prefH == 0 then self.metrics.prefH = 200 end
end

function ActiveTags:drawSelf(tags)
    local w, h = self.props.w, self.props.h
    local lwc = self.lwc
    
    -- Style resolution
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#08081a"))
    local radius = lwc:optNumber("borderRadius", 4)
    local alpha = lwc:optNumber("opacity", 0.8)
    local pad = lwc:optNumber("padding", 8)
    
    System.drawRoundedRect(0, 0, w, h, radius, {bgR, bgG, bgB, alpha})
    
    -- Draw Title
    local title = lwc:optString("title", "ACTIVE TAGS")
    System.drawText(title, pad, pad, 16, {0.5, 0.7, 1.0, alpha})
    
    local ty = 32
    tags = tags or (setbox.getActiveTags and setbox.getActiveTags()) or {}
    
    for _, tag in ipairs(tags) do
        System.drawText(tostring(tag), pad + 4, ty, 14, {1, 1, 1, alpha})
        ty = ty + 18
        if ty > h - 20 then break end
    end
end

return ActiveTags
