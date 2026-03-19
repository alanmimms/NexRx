local Color = require("Color")
local Layout = require("Layout")
local Stick = require("Stick")

local Widget = { typeName = "Widget" }
Widget.__index = Widget

local widgetN = 0
local focusedWidget = nil
local hoveredWidget = nil
local DEFAULT_HIGHLIGHT_COLOR = Color("#FFF")

function Widget.newID()
  widgetN = widgetN + 1
  return "id-" .. widgetN
end

function Widget:init(def)
  def = def or {}
  self.id = def.id or Widget.newID()
  self.name = def.name
  self.type = self.typeName or "Widget"
  self.props = def.props or {}
  
  -- Merge specific def fields into props for convenience (text, value, etc)
  for k, v in pairs(def) do
    if k ~= "props" and k ~= "kids" and k ~= "metrics" and k ~= "tags" and k ~= "parent" then
      self.props[k] = self.props[k] or v
    end
  end

  self.kids = def.kids or {}
  self.layoutFunc = def.layoutFunc
  
  -- LWC (Local Widget Context) for SetBox
  self.lwc = setbox.newContext(def.tags, def.parent and def.parent.lwc)

  self.borderColor = def.borderColor or (self.lwc:has("borderColor") and self.lwc:get("borderColor"))
  self.backgroundColor = def.backgroundColor or (self.lwc:has("backgroundColor") and self.lwc:get("backgroundColor"))
  self.showBorder = (def.showBorder ~= nil) and def.showBorder or (self.lwc:optBool("showBorder", self.borderColor ~= nil))
  self.showBackground = (def.showBackground ~= nil) and def.showBackground or (self.lwc:optBool("showBackground", self.backgroundColor ~= nil))
  
  self.isMouseOver = false
  self.focused = false

  self.metrics = def.metrics or {}
  local m = self.metrics
  m.margin = m.margin or {}
  m.margin.left = m.margin.left or 0
  m.margin.right = m.margin.right or 0
  m.margin.top = m.margin.top or 0
  m.margin.bottom = m.margin.bottom or 0
  
  self.metrics.stick = self.metrics.stick or (Stick.L | Stick.T)
  self.metrics.flexW = self.metrics.flexW or self.lwc:optNumber("flexW", 0)
  self.metrics.flexH = self.metrics.flexH or self.lwc:optNumber("flexH", 0)
  self.metrics.minW = self.metrics.minW or self.lwc:optNumber("minW", 0)
  self.metrics.minH = self.metrics.minH or self.lwc:optNumber("minH", 0)
  self.metrics.prefW = self.metrics.prefW or self.lwc:optNumber("prefW", 0)
  self.metrics.prefH = self.metrics.prefH or self.lwc:optNumber("prefH", 0)
  self.metrics.maxW = self.metrics.maxW or self.lwc:optNumber("maxW", 10000)
  self.metrics.maxH = self.metrics.maxH or self.lwc:optNumber("maxH", 10000)

  self.props.x = self.props.x or 0
  self.props.y = self.props.y or 0
  self.props.w = self.props.w or 0
  self.props.h = self.props.h or 0
  
  for _, kid in ipairs(self.kids) do kid.parent = self end
  if def.focused then self:setFocus() end
end

function Widget.mkType(typeName, base)
  base = base or Widget
  local newT = { typeName = typeName, super = base }
  newT.__index = newT
  local typeMT = {
    __index = base,
    __call = function(theClass, def)
      local instance = setmetatable({}, theClass)
      instance:init(def)
      return instance
    end
  }
  setmetatable(newT, typeMT)
  return newT
end

function Widget:getMetrics() return self.metrics end
function Widget:calcMetrics()
  if self.metrics.prefW == 0 then self.metrics.prefW = 10 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 10 end
end

function Widget:draw()
  if self.showBackground and self.backgroundColor then
    System.drawRect(self.props.x, self.props.y, self.props.w, self.props.h, self.backgroundColor:toTable())
  end

  if (self.showBorder and self.borderColor) or self.isMouseOver then
    local thickness = self.isMouseOver and 3 or 1
    local color = (self.borderColor or DEFAULT_HIGHLIGHT_COLOR):toTable()
    System.drawRectLines(self.props.x, self.props.y, self.props.w, self.props.h, thickness, color)
  end

  for _, kid in ipairs(self.kids) do 
    local ok, err = pcall(kid.draw, kid)
    if not ok then
      print("Error drawing kid", kid.name or kid, kid.id, err)
    end
  end
end

function Widget:layout(x, y, w, h)
  if not self.parent then self:calcMetrics() end
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  if self.layoutFunc then 
    local ok, err = pcall(self.layoutFunc, self)
    if not ok then
      print("Error in layoutFunc for", self.name or self, self.id, err)
    end
  end
end

function Widget:contains(x, y)
  return x >= self.props.x and x <= self.props.x + self.props.w and
         y >= self.props.y and y <= self.props.y + self.props.h
end

function Widget:setFocus()
  if focusedWidget then focusedWidget.focused = false end
  focusedWidget = self
  self.focused = true
end

function Widget.getFocused() return focusedWidget end

function Widget:handleEvent(event)
  local consumed = false
  if self.onEvent then consumed = self.onEvent(self, event) end
  if not consumed and self.parent then return self.parent:handleEvent(event) end
  return consumed
end

function Widget:hitTest(x, y)
  if not self:contains(x, y) then return nil end
  for i = #self.kids, 1, -1 do
    local hit = self.kids[i]:hitTest(x, y)
    if hit then return hit end
  end
  return self
end

function Widget:findByName(name)
  if self.name == name then return self end
  for _, kid in ipairs(self.kids) do
    local found = kid:findByName(name)
    if found then return found end
  end
  return nil
end

function Widget.updateGlobalMouse(root, x, y)
  local hit = root:hitTest(x, y)
  if hit ~= hoveredWidget then
    if hoveredWidget then hoveredWidget.isMouseOver = false end
    hoveredWidget = hit
    if hoveredWidget then hoveredWidget.isMouseOver = true end
  end
  return hit
end

-- --- Container Class ---
local Container = Widget.mkType("Container", Widget)
function Container:init(def)
  Widget.init(self, def)
  if not self.layoutFunc then self.layoutFunc = Layout.hFlow end
end

function Container:calcMetrics()
  local maxW, maxH = 0, 0
  local sumW, sumH = 0, 0
  for _, kid in ipairs(self.kids) do
    kid:calcMetrics()
    local m = kid:getMetrics()
    local kw, kh = m.prefW + m.margin.left + m.margin.right, m.prefH + m.margin.top + m.margin.bottom
    if kw > maxW then maxW = kw end
    if kh > maxH then maxH = kh end
    sumW, sumH = sumW + kw, sumH + kh
  end

  -- XXX these comparisons on layoutFunc values are despicable, scurrilous, and evil.
  if self.metrics.prefW == 0 then
    if self.layoutFunc == Layout.hFlow then self.metrics.prefW = sumW else self.metrics.prefW = maxW end
  end
  if self.metrics.prefH == 0 then
    if self.layoutFunc == Layout.vFlow then self.metrics.prefH = sumH else self.metrics.prefH = maxH end
  end

  if self.metrics.prefW == 0 then self.metrics.prefW = 10 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 10 end
end

function Container:layout(x, y, w, h)
  if not self.parent then self:calcMetrics() end
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  if self.layoutFunc then 
    local ok, err = pcall(self.layoutFunc, self)
    if not ok then
      print("Error in layoutFunc for", self.name or self, self.id, err)
    end
  end
  for _, kid in ipairs(self.kids) do 
    local ok, err = pcall(kid.layout, kid, kid.props.x, kid.props.y, kid.props.w, kid.props.h)
    if not ok then
      print("Error in recursive layout for kid", kid.name or kid, kid.id, err)
    end
  end
end

-- --- Column/Row Classes ---
local Column = Widget.mkType("Column", Container)
function Column:init(def)
  def = def or {}
  def.layoutFunc = def.layoutFunc or Layout.vFlow
  Container.init(self, def)
end

local Row = Widget.mkType("Row", Container)
function Row:init(def)
  def = def or {}
  def.layoutFunc = def.layoutFunc or Layout.hFlow
  Container.init(self, def)
end

-- --- Label Class ---
local Label = Widget.mkType("Label", Widget)
function Label:init(def) 
  Widget.init(self, def)
  self.text = self.props.text or self.lwc:optString("text", self.id)
  self.fontSize = self.props.fontSize or self.lwc:optNumber("fontSize", 20)
end
function Label:calcMetrics()
  local tw = System.measureText(self.text, self.fontSize)
  if self.metrics.prefW == 0 then self.metrics.prefW = tw + 10 end
  if self.metrics.prefH == 0 then self.metrics.prefH = self.fontSize + 10 end
end
function Label:draw()
  Widget.draw(self)
  local stick = self.metrics.stick
  local textW = System.measureText(self.text, self.fontSize)
  local tx, ty = self.props.x + 5, self.props.y + (self.props.h - self.fontSize) / 2
  if (stick & Stick.R) ~= 0 and (stick & Stick.L) ~= 0 then tx = self.props.x + (self.props.w - textW) / 2
  elseif (stick & Stick.R) ~= 0 then tx = self.props.x + self.props.w - textW - 5
  elseif (stick & Stick.L) == 0 then tx = self.props.x + (self.props.w - textW) / 2 end
  if (stick & Stick.B) ~= 0 and (stick & Stick.T) == 0 then ty = self.props.y + self.props.h - self.fontSize - 5
  elseif (stick & Stick.T) ~= 0 and (stick & Stick.B) == 0 then ty = self.props.y + 5 end
  
  local textColor = self.lwc:optString("textColor", "#FFF")
  System.drawText(self.text, tx, ty, self.fontSize, Color(textColor):toTable())
end

-- --- Window Class ---
local Window = Widget.mkType("Window", Container)
function Window:init(def)
  Container.init(self, def)
  self.borderColor = def.borderColor or Color("#444")
  self.backgroundColor = self.borderColor:darken(0.8)
  self.showBackground, self.showBorder = true, true
  self.props.w, self.props.h = def.width or 1280, def.height or 720
end

function Window:layout(x, y, w, h)
  Container.layout(self, x, y, w, h)
end

function Window:calcMetrics() Container.calcMetrics(self) end
function Window:onResize(w, h) self.props.w, self.props.h = w, h end

-- --- Button Class ---
local Button = Widget.mkType("Button", Widget)
function Button:init(def)
  Widget.init(self, def)
  self.text = self.props.text or "Button"
  self.onClicked = self.props.onClicked
end

function Button:calcMetrics()
  local fontSize = self.props.fontSize or 20
  local tw = System.measureText(self.text, fontSize)
  if self.metrics.prefW == 0 then self.metrics.prefW = tw + 20 end
  if self.metrics.prefH == 0 then self.metrics.prefH = fontSize + 15 end
end

function Button:draw()
  local baseColor = self.backgroundColor or Color("#444")
  if self.isMouseOver then
    if self.isDown then
      baseColor = baseColor:darken(0.3)
    else
      baseColor = baseColor:darken(-0.2)
    end
  end
  
  System.drawRect(self.props.x, self.props.y, self.props.w, self.props.h, baseColor:toTable())
  System.drawRectLines(self.props.x, self.props.y, self.props.w, self.props.h, 1, Color("#FFF"):toTable())
  
  local fontSize = self.props.fontSize or 20
  local tw = System.measureText(self.text, fontSize)
  local tx = self.props.x + (self.props.w - tw) / 2
  local ty = self.props.y + (self.props.h - fontSize) / 2
  System.drawText(self.text, tx, ty, fontSize, Color("#FFF"):toTable())
end

function Button:handleEvent(event)
  if event.type == "mouseButton" and event.button == 0 then
    if event.isDown then
      self.isDown = true
      return true
    else
      if self.isDown and self.isMouseOver then
        if self.onClicked then self.onClicked(self) end
      end
      self.isDown = false
      return true
    end
  end
  return Widget.handleEvent(self, event)
end

-- --- BridgeWidget Class (Wraps original app widgets) ---
local BridgeWidget = Widget.mkType("BridgeWidget", Widget)
function BridgeWidget:init(def)
  Widget.init(self, def)
  self.orig = def.orig
  self.id = def.id or self.orig.id or Widget.newID()
  self.tags = def.tags or self.orig.tags or {}
  self.drawArgs = def.drawArgs or {}
end
function BridgeWidget:draw()
  if self.orig and self.orig.draw then
    local x, y, w, h = self.props.x, self.props.y, self.props.w, self.props.h
    self.orig:draw(self.id, x, y, w, h, self.lwc, table.unpack(self.drawArgs))
  end
end

Widget.Container, Widget.Column, Widget.Row, Widget.Label, Widget.Window, Widget.Stick, Widget.Button, Widget.BridgeWidget = Container, Column, Row, Label, Window, Stick, Button, BridgeWidget
return Widget
