--[[
  GraticuleLegend Widget
  Displays scaling info (e.g. "10 kHz/div") in corners of plot widgets.
]]

local setbox = require("SetBox")
local state = require("ui.State")

local GraticuleLegend = {}
GraticuleLegend.__index = GraticuleLegend

-- Default rules
setbox.rule {
    id = "graticule-defaults",
    tags = {"widget.GraticuleLegend"},
    priority = -1000,
    apply = {
        background = "#000000",
        foreground = "#ffffff",
        opacity = 0.5,
        borderRadius = 4,
        padding = 4,
    }
}

function GraticuleLegend.new()
    local self = setmetatable({}, GraticuleLegend)
    return self
end

function GraticuleLegend:draw(id, x, y, w, h, parentLWC, line1, line2)
    local lwc = setbox.newContext({"widget.GraticuleLegend", "id." .. id}, parentLWC)
    
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#000000"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local radius = lwc:optNumber("borderRadius", 4)
    local alpha = lwc:optNumber("opacity", 0.5)
    local pad = lwc:optNumber("padding", 4)
    
    System.drawRoundedRect(x, y, w, h, radius, {bgR, bgG, bgB, alpha})
    
    local ty = y + pad
    if line1 then
        System.drawText(line1, x + pad, ty, 14, {fgR, fgG, fgB, 1.0})
        ty = ty + 16
    end
    if line2 then
        System.drawText(line2, x + pad, ty, 14, {fgR, fgG, fgB, 0.7})
    end
end

return GraticuleLegend
