local Color = require("ui.Color")
local Layout = require("ui.Layout")
local Stick = require("ui.Stick")
local state = require("ui.State")

local Widget = { typeName = "Widget" }
Widget.__index = Widget

local widgetN = 0
local focusedWidget = nil
local hoveredWidget = nil
local DEFAULT_HIGHLIGHT_COLOR = Color("#FFF")

-- Captured original tables if they exist
local RealSystem = _G.__RealSystem or _G.System
local RealWaterfall = _G.__RealWaterfall or _G.waterfall

if not _G.__RealSystem and RealSystem then
    _G.__RealSystem = RealSystem
end
if not _G.__RealWaterfall and RealWaterfall then
    _G.__RealWaterfall = RealWaterfall
end

-- System Wrapper to handle localized drawing
local SystemWrapper = {}
local sysMT = {
    __index = function(_, key)
        local orig = RealSystem and RealSystem[key]
        
        -- If missing, return a no-op function to prevent crashes
        if type(orig) ~= "function" then 
            return function() end 
        end
        
        return function(...)
            local args = {...}
            -- Use state's current accumulated offset
            local ox, oy = state.getOffset()
            
            if key == "drawText" then
                if args[2] then args[2] = args[2] + ox end
                if args[3] then args[3] = args[3] + oy end
            elseif key == "drawLine" then
                if args[1] then args[1] = args[1] + ox end
                if args[2] then args[2] = args[2] + oy end
                if args[3] then args[3] = args[3] + ox end
                if args[4] then args[4] = args[4] + oy end
            elseif key == "drawRect" or key == "drawRectLines" or key == "drawRoundedRect" or key == "drawCircle" or key == "drawCircleOutline" then
                if args[1] then args[1] = args[1] + ox end
                if args[2] then args[2] = args[2] + oy end
            end
            return orig(table.unpack(args))
        end
    end
}
setmetatable(SystemWrapper, sysMT)
_G.System = SystemWrapper

-- Waterfall Wrapper
local WaterfallWrapper = {}
local wfMT = {
    __index = function(_, key)
        local orig = RealWaterfall and RealWaterfall[key]
        if type(orig) ~= "function" then 
            return function() end 
        end
        return function(...)
            local args = {...}
            local ox, oy = state.getOffset()
            if key == "renderSpectrum" then
                if args[2] then args[2] = args[2] + ox end
                if args[3] then args[3] = args[3] + oy end
            elseif key == "render" then
                if args[1] then args[1] = args[1] + ox end
                if args[2] then args[2] = args[2] + oy end
            end
            return orig(table.unpack(args))
        end
    end
}
setmetatable(WaterfallWrapper, wfMT)
_G.waterfall = WaterfallWrapper

function Widget.newID()
  widgetN = widgetN + 1
  return "id-" .. widgetN
end

function Widget:add(kid)
  if not kid then return end
  table.insert(self.kids, kid)
  kid.parent = self
  return kid
end

function Widget:init(def)
  def = def or {}
  self.id = def.id or def.props and def.props.id or Widget.newID()
  self.name = def.name or self.id
  self.type = self.typeName or "Widget"
  self.props = def.props or {}
  
  -- Merge specific def fields into props for convenience (text, value, etc)
  for k, v in pairs(def) do
    if k ~= "props" and k ~= "kids" and k ~= "metrics" and k ~= "tags" and k ~= "parent" then
      if self.props[k] == nil then self.props[k] = v end
    end
  end

  self.kids = def.kids or {}
  self.tags = def.tags or {}
  self.layoutFunc = def.layoutFunc
  self.mouseAction = def.mouseAction
  self.keyAction = def.keyAction
  
  -- LWC (Local Widget Context) for SetBox
  self.lwc = setbox.newContext(def.tags, def.parent and def.parent.lwc)
  if self.id then self.lwc:addTag("id." .. self.id) end

  self.borderColor = def.borderColor or (self.lwc:has("borderColor") and self.lwc:get("borderColor"))
  self.backgroundColor = def.backgroundColor or (self.lwc:has("backgroundColor") and self.lwc:get("backgroundColor"))
  self.showBorder = (def.showBorder ~= nil) and def.showBorder or (self.lwc:optBool("showBorder", self.borderColor ~= nil))
  self.showBackground = (def.showBackground ~= nil) and def.showBackground or (self.lwc:optBool("showBackground", self.backgroundColor ~= nil))
  
  self.isMouseOver = false
  self.focused = false

  self.metrics = def.metrics or {}
  local m = self.metrics

  if type(m.margin) == "number" then
    local v = m.margin
    m.margin = { left = v, right = v, top = v, bottom = v }
  else
    m.margin = m.margin or {}
    m.margin.left = m.margin.left or 0
    m.margin.right = m.margin.right or 0
    m.margin.top = m.margin.top or 0
    m.margin.bottom = m.margin.bottom or 0
  end
  
  if type(m.stick) == "string" then
    m.stick = Stick.mk(m.stick)
  else
    m.stick = m.stick or (Stick.L | Stick.T)
  end
  self.metrics.flexW = self.metrics.flexW or self.lwc:optNumber("flexW", 0)
  self.metrics.flexH = self.metrics.flexH or self.lwc:optNumber("flexH", 0)
  self.metrics.minW = self.metrics.minW or self.lwc:optNumber("minW", 0)
  self.metrics.minH = self.metrics.minH or self.lwc:optNumber("minH", 0)
  self.metrics.prefW = self.metrics.prefW or self.lwc:optNumber("prefW", 0)
  self.metrics.prefH = self.metrics.prefH or self.lwc:optNumber("prefH", 0)
  self.metrics.maxW = self.metrics.maxW or self.lwc:optNumber("maxW", 10000)
  self.metrics.maxH = self.metrics.maxH or self.lwc:optNumber("maxH", 10000)

  self.eventRedirect = def.eventRedirect -- Target widget to redirect events to

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

-- Base rendering logic for a widget. Subclasses should override this
-- instead of draw() to ensure coordinate transformation is handled.
function Widget:drawSelf()
  if self.showBackground and self.backgroundColor then
    System.drawRect(0, 0, self.props.w or 0, self.props.h or 0, self.backgroundColor:toTable())
  end

  if (self.showBorder and self.borderColor) or self.isMouseOver then
    local thickness = self.isMouseOver and 3 or 1
    local color = (self.borderColor or DEFAULT_HIGHLIGHT_COLOR):toTable()
    System.drawRectLines(0, 0, self.props.w or 0, self.props.h or 0, thickness, color)
  end
end

-- Template method for drawing. Handles coordinate transformation and recursion.
function Widget:draw()
  state.pushOffset(self.props.x or 0, self.props.y or 0)

  -- 1. Draw this widget
  local okSelf, errSelf = pcall(self.drawSelf, self)
  if not okSelf then
    print("Error drawing self", self.name or self, self.id, errSelf)
  end

  -- 2. Draw kids
  for _, kid in ipairs(self.kids) do 
    local ok, err = pcall(kid.draw, kid)
    if not ok then
      print("Error drawing kid", kid.name or kid, kid.id, err)
    end
  end

  state.popOffset()
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
  -- x, y are now LOCAL to the widget's origin
  return x >= 0 and x < (self.props.w or 0) and
         y >= 0 and y < (self.props.h or 0)
end

function Widget:setFocus()
  if focusedWidget then focusedWidget.focused = false end
  focusedWidget = self
  self.focused = true
end

function Widget.getFocused() return focusedWidget end
function Widget.getHovered() return hoveredWidget end

local function getEvents()
  return require("Events")
end

function Widget:getAbsolutePos()
  local ax, ay = self.props.x or 0, self.props.y or 0
  local p = self.parent
  while p do
    ax = ax + (p.props.x or 0)
    ay = ay + (p.props.y or 0)
    p = p.parent
  end
  return ax, ay
end

-- Hierarchical Event Handling (Bubbling model)
function Widget:handleEvent(event)
  -- 0. Check for redirection
  if self.eventRedirect and self.eventRedirect ~= self then
    return self.eventRedirect:handleEvent(event)
  end

  -- Create a localized version of the event for self actions
  local localEvent = {}
  for k, v in pairs(event) do localEvent[k] = v end
  
  if localEvent.x and localEvent.y then
    local ax, ay = self:getAbsolutePos()
    localEvent.x = localEvent.x - ax
    localEvent.y = localEvent.y - ay
  end

  local consumed = false
  
  -- 1. Handle via direct action properties
  if localEvent.type:match("^mouse") or localEvent.type:match("^wheel") then
    if self.mouseAction then consumed = self:mouseAction(localEvent) end
  elseif localEvent.type == "key" then
    if self.keyAction then consumed = self:keyAction(localEvent) end
  elseif localEvent.type == "textInput" then
    if self.textAction then consumed = self:textAction(localEvent) end
  end

  -- 2. Bubble if not consumed (using ORIGINAL event coordinates for parent)
  if not consumed and self.parent then 
    return self.parent:handleEvent(event) 
  end
  return consumed
end

function Widget:hitTest(x, y)
  -- x, y are LOCAL to this widget's parent's content area
  -- 1. Is it inside me?
  if not self:contains(x, y) then return nil end
  
  -- 2. Check kids (localized to ME)
  local localX, localY = x, y -- Already localized by parent
  for i = #self.kids, 1, -1 do
    local kid = self.kids[i]
    local hit = kid:hitTest(localX - (kid.props.x or 0), localY - (kid.props.y or 0))
    if hit then return hit end
  end
  
  -- if self.name ~= "Main Window" then print("[Widget] hitTest SUCCESS:", self.name) end
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
  -- Root starts at screen 0,0
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
    local marginW = (m.margin.left or 0) + (m.margin.right or 0)
    local marginH = (m.margin.top or 0) + (m.margin.bottom or 0)
    local kw, kh = (m.prefW or 0) + marginW, (m.prefH or 0) + marginH
    if kw > maxW then maxW = kw end
    if kh > maxH then maxH = kh end
    sumW, sumH = sumW + kw, sumH + kh
  end

  local axis = self.layoutFunc and self.layoutFunc.axis
  if self.metrics.prefW == 0 then
    if axis == "horizontal" then self.metrics.prefW = sumW else self.metrics.prefW = maxW end
  end
  if self.metrics.prefH == 0 then
    if axis == "vertical" then self.metrics.prefH = sumH else self.metrics.prefH = maxH end
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
  local tw = System.measureText(tostring(self.text), self.fontSize)
  if self.metrics.prefW == 0 then self.metrics.prefW = tw + 10 end
  if self.metrics.prefH == 0 then self.metrics.prefH = self.fontSize + 10 end
end
function Label:drawSelf()
  Widget.drawSelf(self)
  local stick = self.metrics.stick
  local textW = System.measureText(tostring(self.text), self.fontSize)
  local tx, ty = 5, ((self.props.h or 0) - self.fontSize) / 2
  if (stick & Stick.R) ~= 0 and (stick & Stick.L) ~= 0 then tx = ((self.props.w or 0) - textW) / 2
  elseif (stick & Stick.R) ~= 0 then tx = (self.props.w or 0) - textW - 5
  elseif (stick & Stick.L) == 0 then tx = ((self.props.w or 0) - textW) / 2 end
  if (stick & Stick.B) ~= 0 and (stick & Stick.T) == 0 then ty = (self.props.h or 0) - self.fontSize - 5
  elseif (stick & Stick.T) ~= 0 and (stick & Stick.B) == 0 then ty = 5 end
  
  local textColor = self.lwc:optString("textColor", "#FFF")
  System.drawText(tostring(self.text), tx, ty, self.fontSize, Color(textColor):toTable())
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
  local tw = System.measureText(tostring(self.text), fontSize)
  if self.metrics.prefW == 0 then self.metrics.prefW = tw + 20 end
  if self.metrics.prefH == 0 then self.metrics.prefH = fontSize + 15 end
end

function Button:drawSelf()
  local baseColor = self.backgroundColor or Color("#444")
  local selected = self.props.selected or self.selected
  
  if selected then
    baseColor = Color("#facc15") -- Yellow highlight for selected
  elseif self.isMouseOver then
    if self.isDown then
      baseColor = baseColor:darken(0.3)
    else
      baseColor = baseColor:darken(-0.2)
    end
  end
  
  local textColor = selected and Color("#000") or Color("#FFF")
  
  -- Use local 0,0
  System.drawRect(0, 0, self.props.w or 0, self.props.h or 0, baseColor:toTable())
  
  local fontSize = self.props.fontSize or 20
  local tw = System.measureText(tostring(self.text), fontSize)
  local tx = ((self.props.w or 0) - tw) / 2
  local ty = ((self.props.h or 0) - fontSize) / 2
  System.drawText(tostring(self.text), tx, ty, fontSize, textColor:toTable())
end

function Button:handleEvent(event)
  if event.type == "mouseButton" and event.button == "LEFT" then
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

Widget.Container = Container
Widget.Column = Column
Widget.Row = Row
Widget.Label = Label
Widget.Window = Window
Widget.Stick = Stick
Widget.Button = Button
return Widget
