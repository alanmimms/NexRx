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

function SignalBox:draw(id, x, y, w, h, parentLWC, label, extraTags, data)
    local tags = {"widget.SignalBox", "id." .. id}
    if extraTags then
        if type(extraTags) == "table" then
            for _, t in ipairs(extraTags) do table.insert(tags, t) end
        end
    end
    
    -- Interaction tags (state namespaces)
    if state.isActive(id) then table.insert(tags, "state.Active")
    elseif state.isHot(id) then table.insert(tags, "state.Hovered") end

    local lwc = setbox.newContext(tags, parentLWC)
    
    -- Register widget for event system (local bounds)
    state.registerWidget(id, {x=x, y=y, w=w, h=h}, tags, data)
    
    -- Hit testing (Relative to parent origin)
    if state.pointInRect(state.mouseX, state.mouseY, x, y, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end

    local isGhost = lwc:hasTag("state.Ghost")
    local isSelected = lwc:hasTag("state.Selected")
    
    -- Style resolution
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#facc15"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#facc15"))
    
    local alpha = lwc:optNumber("opacity", 0.3)
    local bWidth = lwc:optNumber("borderWidth", 1)
    
    if isSelected then
        alpha = lwc:optNumber("selectedOpacity", 0.6)
        bWidth = lwc:optNumber("selectedBorderWidth", 3)
    elseif isGhost then
        alpha = lwc:optNumber("ghostOpacity", 0.2)
    end
    
    -- 1. Draw the translucent box (the signal area)
    System.drawRect(x, y, w, h, {bgR, bgG, bgB, alpha})
    
    -- 2. Draw border
    if bWidth > 0 then
        System.drawLine(x, y, x, y + h, bWidth, {bR, bG, bB, alpha})
        System.drawLine(x + w, y, x + w, y + h, bWidth, {bR, bG, bB, alpha})
        System.drawLine(x, y, x + w, y, bWidth, {bR, bG, bB, alpha})
    end
    
    -- 3. Draw the numeric tag above the box
    local tagH = lwc:optNumber("tagHeight", 20)
    local tagW = lwc:optNumber("tagWidth", 30)
    local tx = x + (w - tagW) / 2
    local ty = y - tagH - 2
    
    System.drawRoundedRect(tx, ty, tagW, tagH, 4, {bgR, bgG, bgB, 1.0})
    
    local labelW = System.measureText(tostring(label), 14)
    local lineH = 14
    System.drawText(tostring(label), tx + (tagW - labelW) / 2, ty + (tagH - lineH) / 2, 14, {fgR, fgG, fgB, 1.0})
end

return SignalBox
