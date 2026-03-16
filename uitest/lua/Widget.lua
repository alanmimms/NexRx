local Color = require("Color")
local Layout = require("Layout")

-- --- Stick Constants ---
local Stick = {
  L = 1, R = 2, T = 4, B = 8
}

function Stick.mk(s)
   if s == "all" then
      return 15
   end

   local mask = 0
  
   for c in s:gmatch(".") do
      mask = mask | Stick[c]
   end

   return mask
end

_G.Stick = Stick -- Make global for Layout.lua or require it there if preferred

local Widget = {}
Widget.__index = Widget

function Widget.new(def)
  local self = setmetatable({}, Widget)
  self:init(def)
  return self
end

function Widget:init(def)
  self.id = def.id or "widget"
  self.type = def.type or "Widget"
  self.props = def.props or {}
  self.kids = def.children or {} -- Changed from 'children' to 'kids' to match Layout.lua
  self.layoutFunc = def.layout or nil
  self.borderColor = def.borderColor or Color("#FFF")
  self.backgroundColor = self.borderColor:darken(0.8)
  
  -- Metrics for Layout.lua
  self.metrics = def.metrics or {}
  self.metrics.margin = self.metrics.margin or { left = 0, right = 0, top = 0, bottom = 0 }
  self.metrics.stick = self.metrics.stick or (Stick.L | Stick.T)
  self.metrics.flexW = self.metrics.flexW or 0
  self.metrics.flexH = self.metrics.flexH or 0
  self.metrics.minW = self.metrics.minW or 0
  self.metrics.minH = self.metrics.minH or 0
  self.metrics.prefW = self.metrics.prefW or 100
  self.metrics.prefH = self.metrics.prefH or 100
  self.metrics.maxW = self.metrics.maxW or 10000
  self.metrics.maxH = self.metrics.maxH or 10000

  -- Final geometry updated by Layout.lua
  -- Note: Layout.lua uses kid.props.x, kid.props.y, kid.props.w, kid.props.h
  self.props.x = self.props.x or 0
  self.props.y = self.props.y or 0
  self.props.w = self.props.w or 0
  self.props.h = self.props.h or 0
  
  for _, kid in ipairs(self.kids) do kid.parent = self end
end

function Widget:getMetrics()
  return self.metrics
end

function Widget:draw(bridge)
  bridge.drawRect(self.props.x, self.props.y, self.props.w, self.props.h, self.backgroundColor:toTable())
  bridge.drawRectLines(self.props.x, self.props.y, self.props.w, self.props.h, 1, self.borderColor:toTable())
  for _, kid in ipairs(self.kids) do kid:draw(bridge) end
end

function Widget:layout(x, y, w, h)
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  if self.layoutFunc then 
    self.layoutFunc(self) 
  end
  -- Recursively layout kids
  for _, kid in ipairs(self.kids) do
    if kid.layoutFunc then
      -- If kid is a container, it will call its own layoutFunc during its layout call
      -- but we need to trigger it. Actually, Container:layout below handles it.
    end
  end
end

-- --- Container Class ---
local Container = setmetatable({}, { __index = Widget })
Container.__index = Container

function Container.new(def)
  local self = setmetatable({}, Container)
  self:init(def)
  self.direction = def.direction or "vertical"
  if not def.layout then
    if self.direction == "vertical" then
      self.layoutFunc = Layout.layoutVFlow
    else
      self.layoutFunc = Layout.layoutHFlow
    end
  end
  return self
end

function Container:layout(x, y, w, h)
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  if self.layoutFunc then
    self.layoutFunc(self)
  end
  -- Containers must recursively layout their kids
  for _, kid in ipairs(self.kids) do
    kid:layout(kid.props.x, kid.props.y, kid.props.w, kid.props.h)
  end
end

-- --- Label Class ---
local Label = setmetatable({}, { __index = Widget })
Label.__index = Label

function Label.new(def)
  local self = setmetatable({}, Label)
  self:init(def)
  self.text = self.props.text or self.id
  return self
end

function Label:draw(bridge)
  Widget.draw(self, bridge)
  local fontSize = 20
  local textW = bridge.measureText(self.text, fontSize)
  local stick = self.metrics.stick
  local tx = self.props.x + 5
  local ty = self.props.y + (self.props.h - fontSize) / 2
  
  if (stick & Stick.R) ~= 0 and (stick & Stick.L) == 0 then
    tx = self.props.x + self.props.w - textW - 5
  elseif (stick & Stick.R) ~= 0 and (stick & Stick.L) ~= 0 then
    tx = self.props.x + (self.props.w - textW) / 2
  elseif (stick & Stick.R) == 0 and (stick & Stick.L) == 0 then
    tx = self.props.x + (self.props.w - textW) / 2
  end
  
  bridge.drawText(self.text, tx, ty, fontSize, Color("#FFF"):toTable())
end

-- --- Window Class ---
local Window = setmetatable({}, { __index = Container })
Window.__index = Window

function Window.new(def)
  def.direction = "horizontal"
  local self = Container.new(def)
  setmetatable(self, Window)
  self.borderColor = def.borderColor or Color("#444")
  self.backgroundColor = self.borderColor:darken(0.8)
  self.props.w = def.width or 1280
  self.props.h = def.height or 720
  return self
end

function Window:onResize(w, h)
  self.props.w = w
  self.props.h = h
end

-- Export Factory Methods under Widget table
Widget.Window = function(def) return Window.new(def) end
Widget.Container = function(def) return Container.new(def) end
Widget.Label = function(def) return Label.new(def) end
Widget.VerticalColumn = function(def) def.direction = "vertical"; return Container.new(def) end
Widget.HorizontalRow = function(def) def.direction = "horizontal"; return Container.new(def) end
Widget.Stick = Stick

return Widget
