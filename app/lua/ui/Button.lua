--[[
  Button Widget
  Modular UI component for clickable actions.
  Behavior and style driven entirely by SetBox rules.
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
    }
}

function Button:init(def)
    Widget.init(self, def)
    self.getText = def.getText
    self.onClick = def.onClick
end

function Button:calcMetrics()
    local lwc = self.lwc
    if self.metrics.prefW == 0 then self.metrics.prefW = lwc:optNumber("width", 100) end
    if self.metrics.prefH == 0 then self.metrics.prefH = lwc:optNumber("height", 32) end
end

function Button:drawSelf(extraTags)
    local id, w, h = self.id, self.props.w, self.props.h
    local tags = {"widget.Button", "id." .. id}
    if extraTags then
        if type(extraTags) == "table" then
            for _, t in ipairs(extraTags) do table.insert(tags, t) end
        end
    end
    
    -- Interaction tags (state namespaces)
    if state.isActive(id) then table.insert(tags, "state.Pressed")
    elseif state.isHot(id) then table.insert(tags, "state.Hovered") end
    
    local lwc = self.lwc
    
    state.registerWidget(id, {x=0, y=0, w=w, h=h}, tags)
    
    -- Hit testing and state update (Local 0,0)
    if state.pointInRect(state.mouseX, state.mouseY, 0, 0, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end
    
    -- Style resolution
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#3b82f6"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#2563eb"))
    local bWidth = lwc:optNumber("borderWidth", 1)
    local radius = lwc:optNumber("borderRadius", 6)
    local alpha = lwc:optNumber("opacity", 1.0)
    
    -- Draw (Local 0,0)
    System.drawRoundedRect(0, 0, w, h, radius, {bgR, bgG, bgB, alpha})
    if bWidth > 0 then
        System.drawRectLines(0, 0, w, h, bWidth, {bR, bG, bB, alpha})
    end
    
    -- Label text from callback or rule
    local label = ""
    if self.getText then
        label = self.getText()
    elseif lwc:has("label") then
        label = lwc:getString("label")
    elseif lwc:has("text") then
        label = lwc:getString("text")
    else
        label = id
    end
    
    local labelW = TextMixin.measure(label)
    local lineH = TextMixin.getLineHeight()
    TextMixin.draw((w - labelW) / 2, (h - lineH) / 2, label, lwc)
    
    if state.wasClicked(id) then
        if self.onClick then self.onClick() end
    end
end

return Button
