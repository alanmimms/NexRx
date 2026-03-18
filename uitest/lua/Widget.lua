local Color = require("Color")
local Layout = require("Layout")
local Stick = require("Stick")

local Widget = { typeName = "Widget" }
Widget.__index = Widget

local widgetN = 0

function Widget.newID()
  widgetN = widgetN + 1
  return "id-" .. string.format("%09d", widgetN)
end

function Widget:init(def)
  def = def or {}
  self.id = def.id or Widget.newID()
  self.type = self.typeName or "Widget"
  self.props = def.props or {}
  self.tags = def.tags or {}
  self.kids = def.kids or {}
  self.layoutFunc = def.layoutFunc or nil
  
  -- Rendering properties
  self.borderColor = def.borderColor or nil
  self.backgroundColor = def.backgroundColor or nil
  self.showBorder = def.showBorder or (self.borderColor ~= nil)
  self.showBackground = def.showBackground or (self.backgroundColor ~= nil)
  
  -- Metrics for Layout.lua
  self.metrics = def.metrics or {}
  self.metrics.margin = self.metrics.margin or { left = 0, right = 0, top = 0, bottom = 0 }
  self.metrics.stick = self.metrics.stick or (Stick.L | Stick.T)
  self.metrics.flexW = self.metrics.flexW or 0
  self.metrics.flexH = self.metrics.flexH or 0
  self.metrics.minW = self.metrics.minW or 0
  self.metrics.minH = self.metrics.minH or 0
  self.metrics.prefW = self.metrics.prefW or 0
  self.metrics.prefH = self.metrics.prefH or 0
  self.metrics.maxW = self.metrics.maxW or 10000
  self.metrics.maxH = self.metrics.maxH or 10000

  -- Final geometry updated by Layout.lua
  self.props.x = self.props.x or 0
  self.props.y = self.props.y or 0
  self.props.w = self.props.w or 0
  self.props.h = self.props.h or 0
  
  for _, kid in ipairs(self.kids) do 
    kid.parent = self 
  end
end

-- Factory to take type name and create a new Widget type that calls
-- its own init function.
function Widget.mkType(typeName, base)
  base = base or Widget
  local newT = { typeName = typeName, super = base }
  
  -- Instances will look up methods in this specific type table
  newT.__index = newT

  local typeMT = {
    -- Allow the type itself to inherit class-level methods from base
    __index = base,
    
    -- The constructor invoked when calling MyNewWidgetType{ ... }
    __call = function(theClass, def)
      local instance = setmetatable({}, theClass)
      instance:init(def)
      return instance
    end
  }

  setmetatable(newT, typeMT)
  return newT
end

function Widget:getMetrics()
  return self.metrics
end

function Widget:calcMetrics(bridge)
  if self.metrics.prefW == 0 then self.metrics.prefW = 10 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 10 end
end

function Widget:draw(bridge)
  if self.showBackground and self.backgroundColor then
    bridge.drawRect(self.props.x, self.props.y, self.props.w, self.props.h, self.backgroundColor:toTable())
  end
  if self.showBorder and self.borderColor then
    bridge.drawRectLines(self.props.x, self.props.y, self.props.w, self.props.h, 1, self.borderColor:toTable())
  end
  for _, kid in ipairs(self.kids) do kid:draw(bridge) end
end

function Widget:layout(bridge, x, y, w, h)
  if not bridge then return end
  -- Root layout call should trigger recursive calcMetrics
  if not self.parent then
    self:calcMetrics(bridge)
  end
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  if self.layoutFunc then 
    self.layoutFunc(self) 
  end
end

-- --- Container Class ---
local Container = Widget.mkType("Container", Widget)

function Container:init(def)
  Widget.init(self, def)
  if not self.layoutFunc then
    self.layoutFunc = Layout.hFlow
  end
end

function Container:calcMetrics(bridge)
  local maxW, maxH = 0, 0
  local sumW, sumH = 0, 0
  
  for _, kid in ipairs(self.kids) do
    kid:calcMetrics(bridge)
    local m = kid:getMetrics()
    local kw = m.prefW + m.margin.left + m.margin.right
    local kh = m.prefH + m.margin.top + m.margin.bottom
    
    if kw > maxW then maxW = kw end
    if kh > maxH then maxH = kh end
    sumW = sumW + kw
    sumH = sumH + kh
  end
  
  -- Use calculated values if not explicitly set by user (still 0)
  if self.metrics.prefW == 0 then
    if self.layoutFunc == Layout.hFlow then self.metrics.prefW = sumW
    elseif self.layoutFunc == Layout.vFlow then self.metrics.prefW = maxW
    else self.metrics.prefW = maxW end
  end
  
  if self.metrics.prefH == 0 then
    if self.layoutFunc == Layout.hFlow then self.metrics.prefH = maxH
    elseif self.layoutFunc == Layout.vFlow then self.metrics.prefH = sumH
    else self.metrics.prefH = maxH end
  end

  -- Fallback
  if self.metrics.prefW == 0 then self.metrics.prefW = 10 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 10 end
end

function Container:layout(bridge, x, y, w, h)
  if not bridge then return end
  if not self.parent then
    self:calcMetrics(bridge)
  end
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  if self.layoutFunc then
    self.layoutFunc(self)
  end
  -- Containers must recursively layout their kids
  for _, kid in ipairs(self.kids) do
    kid:layout(bridge, kid.props.x, kid.props.y, kid.props.w, kid.props.h)
  end
end

-- --- Label Class ---
local Label = Widget.mkType("Label", Widget)

function Label:init(def)
  Widget.init(self, def)
  self.text = self.props.text or self.id
end

function Label:calcMetrics(bridge)
  local fontSize = self.props.fontSize or 20
  local tw = bridge.measureText(self.text, fontSize)
  -- Only update if not explicitly set
  if self.metrics.prefW == 0 then self.metrics.prefW = tw + 10 end
  if self.metrics.prefH == 0 then self.metrics.prefH = fontSize + 10 end
end

function Label:draw(bridge)
  Widget.draw(self, bridge)
  local fontSize = self.props.fontSize or 20
  local textW = bridge.measureText(self.text, fontSize)
  local stick = self.metrics.stick
  local tx = self.props.x + 5
  local ty = self.props.y + (self.props.h - fontSize) / 2
  
  if (stick & Stick.R) ~= 0 and (stick & Stick.L) ~= 0 then
    tx = self.props.x + (self.props.w - textW) / 2
  elseif (stick & Stick.R) ~= 0 and (stick & Stick.L) == 0 then
    tx = self.props.x + self.props.w - textW - 5
  elseif (stick & Stick.R) == 0 and (stick & Stick.L) == 0 then
    tx = self.props.x + (self.props.w - textW) / 2
  end

  if (stick & Stick.B) ~= 0 and (stick & Stick.T) == 0 then
    ty = self.props.y + self.props.h - fontSize - 5
  elseif (stick & Stick.T) ~= 0 and (stick & Stick.B) == 0 then
    ty = self.props.y + 5
  end
  
  bridge.drawText(self.text, tx, ty, fontSize, Color("#FFF"):toTable())
end

-- --- Window Class ---
local Window = Widget.mkType("Window", Container)

function Window:init(def)
  Container.init(self, def)
  self.borderColor = def.borderColor or Color("#444")
  self.backgroundColor = self.borderColor:darken(0.8)
  self.showBackground = true
  self.showBorder = true
  self.props.w = def.width or 1280
  self.props.h = def.height or 720
end

function Window:calcMetrics(bridge)
  Container.calcMetrics(self, bridge)
end

function Window:onResize(w, h)
  self.props.w = w
  self.props.h = h
end

-- Export Factory Methods and Classes under Widget table
Widget.Container = Container
Widget.Label = Label
Widget.Window = Window
Widget.Column = function(def) 
  def = def or {}
  def.layoutFunc = Layout.vFlow
  return Container(def) 
end
Widget.Row = function(def) 
  def = def or {}
  def.layoutFunc = Layout.hFlow
  return Container(def) 
end
Widget.Stick = Stick

return Widget
