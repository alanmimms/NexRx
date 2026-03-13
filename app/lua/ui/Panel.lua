--[[
  Panel Widget
  Modular UI component for containers and backgrounds.
  Style and behavior driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")

local Panel = {}
Panel.__index = Panel

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

function Panel.new()
    local self = setmetatable({}, Panel)
    return self
end

function Panel:draw(id, x, y, w, h, parentLWC)
    local lwc = setbox.newContext({"widget.Panel", "id." .. id}, parentLWC)
    
    -- Style resolution from rules
    local bgR, bgG, bgB = state.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = state.hexToRgb(lwc:getString("border"))
    local bWidth = lwc:getNumber("borderWidth")
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    
    -- Draw
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end
end

return Panel
