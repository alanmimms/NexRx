--[[
  UI Widgets Prototype System
  All widgets inherit from the base Widget prototype.
  All data and behavior are driven by SetBox rules.
]]

local setbox = require("SetBox")
local Reactive = require("Reactive")

_G.Widget = {}
Widget.__index = Widget

function Widget.new(id, tags)
    local self = setmetatable({name = "Widget"}, Widget)
    self.id = id
    self.tags = tags or {}
    table.insert(self.tags, "id." .. id)
    
    -- Local Widget Context (LWC) from SetBox
    -- This will be used to resolve all properties reactively
    self.lwc = setbox.newContext(self.tags)
    
    -- Common observable state could go here if needed,
    -- but user wants SetBox to drive everything.
    return self
end

function Widget:get(name) return self.lwc:get(name) end
function Widget:has(name) return self.lwc:has(name) end

function Widget:draw(x, y, w, h)
    -- Base widget just draws a background if defined
    if self:has("backgroundColor") then
        local c = self:get("backgroundColor")
        drawRect(x, y, w, h, c[1], c[2], c[3], c[4] or 1.0)
    end
    
    -- Optional outline for debugging layout
    if self:has("outlineColor") then
        local c = self:get("outlineColor")
        drawRectOutline(x, y, w, h, c[1], c[2], c[3], c[4] or 1.0, 1.0)
    end
end

function Widget.getProps(id, tags)
    local lwc = setbox.newContext(tags)
    local props = {}
    local propNames = {
        "parent", "width", "height", "minWidth", "minHeight",
        "springLeft", "springRight", "springTop", "springBottom",
        "spacing", "padding", "order", "direction", "label"
    }

    for _, name in ipairs(propNames) do
        local val = lwc:getRaw(name)
        if val ~= nil then props[name] = val end
    end
    props._lwc = lwc
    return props
end



-- =============================================================================
-- WindowWidget
-- =============================================================================
local WindowWidget = setmetatable({name = "Window"}, { __index = Widget })
WindowWidget.__index = WindowWidget

function WindowWidget.new(id, tags)
    local t = tags or {}
    return setmetatable(Widget.new(id, t), WindowWidget)
end

-- =============================================================================
-- CompoundWidget
-- =============================================================================
local CompoundWidget = setmetatable({name = "Compound"}, { __index = Widget })
CompoundWidget.__index = CompoundWidget

function CompoundWidget.new(id, tags)
    local t = tags or {}
    table.insert(t, "widget.Compound")
    return setmetatable(Widget.new(id, t), CompoundWidget)
end

function CompoundWidget:draw(x, y, w, h)
    Widget.draw(self, x, y, w, h)
end


-- =============================================================================
-- LabelWidget (Leaf)
-- =============================================================================
local LabelWidget = setmetatable({name = "Label"}, { __index = Widget })
LabelWidget.__index = LabelWidget

function LabelWidget.new(id, tags)
    local t = tags or {}
    table.insert(t, "widget.Label")
    table.insert(t, "widget.Leaf")
    return setmetatable(Widget.new(id, t), LabelWidget)
end

function LabelWidget:draw(x, y, w, h)
    Widget.draw(self, x, y, w, h)
    
    local label = self:get("label")
    local color = self:get("textColor")
    
    -- Center text in the widget area
    local tw = measureText(label)
    local th = getLineHeight()
    local tx = x + (w - tw) / 2
    local ty = y + (h - th) / 2
    
    drawText(tx, ty, label, color[1], color[2], color[3], color[4] or 1.0)
end

return {
    Widget = Widget,
    Window = WindowWidget,
    Compound = CompoundWidget,
    Label = LabelWidget
}
