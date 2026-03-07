--[[
  Graticule Legend Widget
  Modular UI component for displaying graticule scale information.
  Style and text driven entirely by SetBox rules.
]]

local setbox = require("setbox")
local Label = require("ui.Label")

local GraticuleLegend = {}
GraticuleLegend.__index = GraticuleLegend

-- Default rules for GraticuleLegend widget (very low priority)
setbox.rule {
    id = "graticule-legend-defaults",
    tags = {"widget.GraticuleLegend"},
    priority = -1000,
    apply = {
        background = "#0f172a",
        border = "#334155",
        borderWidth = 1,
        borderRadius = 4,
        padding = 6,
        opacity = 0.8,
    }
}

-- Default rules for nested labels
setbox.rule {
    id = "legend-h-text-default",
    tags = {"widget.GraticuleLegend", "legend.hText"},
    priority = -500,
    apply = {
        foreground = "#94a3b8",
        text = "Scale H",
    }
}

setbox.rule {
    id = "legend-v-text-default",
    tags = {"widget.GraticuleLegend", "legend.vText"},
    priority = -500,
    apply = {
        foreground = "#94a3b8",
        text = "Scale V",
    }
}

function GraticuleLegend.new()
    local self = setmetatable({}, GraticuleLegend)
    self.hLabel = Label.new()
    self.vLabel = Label.new()
    return self
end

function GraticuleLegend:draw(id, x, y, w, h, hText, vText, parentLWC)
    local lwc = setbox.newContext({"widget.GraticuleLegend", "id." .. id}, parentLWC)
    
    -- Properties from rules
    local bgR, bgG, bgB = require("ui.widgets").hexToRgb(lwc:getString("background"))
    local bR, bG, bB = require("ui.widgets").hexToRgb(lwc:getString("border"))
    local bWidth = lwc:getNumber("borderWidth")
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local pad = lwc:getNumber("padding")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end
    
    -- Draw Scale Labels
    if hText then self.hLabel.getText = function() return hText end end
    self.hLabel:draw(id .. "-h", x + pad, y + pad, lwc)
    
    if vText then self.vLabel.getText = function() return vText end end
    self.vLabel:draw(id .. "-v", x + pad, y + pad + 18, lwc)
end

return GraticuleLegend
