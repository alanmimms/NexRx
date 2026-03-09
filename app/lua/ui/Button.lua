--[[
  Button Widget
  Modular UI component for clickable actions.
  Behavior and style driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")

local Button = {}
Button.__index = Button

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
    }
}

function Button.new(options)
    local self = setmetatable({}, Button)
    -- options.getText is an optional callback () -> string
    if options and options.getText then
        self.getText = options.getText
    end
    self.onClick = options and options.onClick
    return self
end

function Button:draw(id, x, y, w, h, extraTags, parentLWC)
    local tags = {"widget.Button", "id." .. id}
    if extraTags then
        for _, t in ipairs(extraTags) do table.insert(tags, t) end
    end
    
    -- Interaction tags (state namespaces)
    if state.isActive(id) then table.insert(tags, "state.Pressed")
    elseif state.isHot(id) then table.insert(tags, "state.Hovered") end
    
    local lwc = setbox.newContext(tags, parentLWC)
    
    -- Size from rules if not explicitly passed
    w = w or lwc:getNumber("width")
    h = h or lwc:getNumber("height")
    
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, tags)
    
    -- Hit testing and state update
    if state.pointInRect(state.mouseX, state.mouseY, x, y, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end
    
    -- Style resolution
    local bgR, bgG, bgB = require("ui.Widgets").hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = require("ui.Widgets").hexToRgb(lwc:getString("foreground"))
    local bR, bG, bB = require("ui.Widgets").hexToRgb(lwc:getString("border"))
    local bWidth = lwc:getNumber("borderWidth")
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    
    -- Draw
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end
    
    -- Label text from callback or rule
    local label = ""
    if self.getText then
        label = self.getText()
    elseif lwc:has("label") then
        label = lwc:getString("label")
    else
        label = id
    end
    
    local labelW = measureText(label)
    local lineH = getLineHeight()
    drawText(x + (w - labelW) / 2, y + (h - lineH) / 2, label, fgR, fgG, fgB, alpha)
    
    local clicked = state.wasClicked(id)
    if clicked and self.onClick then self.onClick() end
    return clicked
end

return Button
