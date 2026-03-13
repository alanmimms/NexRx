--[[
  SignalBox Widget
  Visual representation of a selected frequency range in the spectrum.
]]

local setbox = require("SetBox")
local state = require("ui.State")

local SignalBox = {}
SignalBox.__index = SignalBox

-- Default rules for SignalBox widget
setbox.rule {
    id = "signalbox-defaults",
    tags = {"widget.SignalBox"},
    priority = -1000,
    apply = {
        background = "#facc15",
        foreground = "#ffffff",
        border = "#facc15",
        borderWidth = 1,
        opacity = 0.3,
        selectedOpacity = 0.6,
        selectedBorderWidth = 3,
        ghostOpacity = 0.2,
        tagHeight = 20,
        tagWidth = 30,
    }
}

function SignalBox.new()
    local self = setmetatable({}, SignalBox)
    return self
end

function SignalBox:draw(id, x, y, w, h, parentLWC, label, extraTags)
    local tags = {"widget.SignalBox", "id." .. id}
    if extraTags then
        if type(extraTags) == "table" then
            for _, t in ipairs(extraTags) do table.insert(tags, t) end
        end
    end
    
    -- Register widget for event system
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, tags)
    
    -- Hit testing
    if state.pointInRect(state.mouseX, state.mouseY, x, y, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end

    local lwc = setbox.newContext(tags, parentLWC)
    
    -- Interaction tags (state namespaces)
    local isGhost = lwc:hasTag("state.Ghost")
    local isSelected = lwc:hasTag("state.Selected")
    
    -- Style resolution
    local bgR, bgG, bgB = state.hexToRgb(lwc:getString("background"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:getString("foreground"))
    local bR, bG, bB = state.hexToRgb(lwc:getString("border"))
    
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    if isSelected then
        alpha = lwc:getNumber("selectedOpacity")
        bWidth = lwc:getNumber("selectedBorderWidth")
    elseif isGhost then
        alpha = lwc:getNumber("ghostOpacity")
    end
    
    -- 1. Draw the translucent box (the signal area)
    drawRect(x, y, w, h, bgR, bgG, bgB, alpha)
    
    -- 2. Draw border
    if bWidth > 0 then
        -- We draw vertical lines at the edges and a top line
        drawLine(x, y, x, y + h, bR, bG, bB, alpha, bWidth)
        drawLine(x + w, y, x + w, y + h, bR, bG, bB, alpha, bWidth)
        drawLine(x, y, x + w, y, bR, bG, bB, alpha, bWidth)
    end
    
    -- 3. Draw the numeric tag above the box
    local tagH = lwc:getNumber("tagHeight")
    local tagW = lwc:getNumber("tagWidth")
    local tx = x + (w - tagW) / 2
    local ty = y - tagH - 2
    
    drawRoundedRect(tx, ty, tagW, tagH, 4, bgR, bgG, bgB, 1.0)
    
    local labelW = measureText(tostring(label))
    local lineH = getLineHeight()
    drawText(tx + (tagW - labelW) / 2, ty + (tagH - lineH) / 2, tostring(label), fgR, fgG, fgB, 1.0)
end

return SignalBox
