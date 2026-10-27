local Color = require("ui.Color")
local Layout = require("ui.Layout")
local Stick = require("ui.Stick")
local state = require("ui.State")
local R = require("Reactive")
local setbox = require("SetBox")

local Widget = { typeName = "Widget" }
Widget.__index = Widget

local widgetN = 0
local focusedWidget = nil
local hoveredWidget = nil
local DEFAULT_HIGHLIGHT_COLOR = Color("#FFF")

-- Base default rule for all Widgets
local function ensureBaseDefaults()
  for _, r in ipairs(setbox.getRules()) do
    if r.id == "widget-base-defaults" then
      return
    end
  end
  setbox.rule {
    id = "widget-base-defaults",
    tags = { "widget.Widget" },
    priority = -1000,
    apply = {
      flexW = 0,
      flexH = 0,
      minW = 0,
      minH = 0,
      prefW = 0,
      prefH = 0,
      maxW = 10000,
      maxH = 10000,
      margin = { left = 0, right = 0, top = 0, bottom = 0 },
      stick = Stick.L | Stick.T,
      opacity = 1.0,
    }
  }
end
ensureBaseDefaults()

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

    -- If missing, return a safe fallback function
    if type(orig) ~= "function" then
      if key == "measureText" then
        return function(txt, sz) return #(tostring(txt or "")) * 8, sz or 16 end
      end
      return function() end
    end

    return function(...)
      local args = { ... }
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
      local args = { ... }
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

local function normalizeMargin(val)
  if type(val) == "number" then
    return { left = val, right = val, top = val, bottom = val }
  elseif type(val) == "table" then
    return {
      left = val.left or 0,
      right = val.right or 0,
      top = val.top or 0,
      bottom = val.bottom or 0,
    }
  end
  return { left = 0, right = 0, top = 0, bottom = 0 }
end

local function normalizeStick(val)
  if type(val) == "string" then
    return Stick.mk(val)
  elseif type(val) == "number" then
    return val
  end
  return Stick.L | Stick.T
end

local function normalizePropValue(key, val)
  if val == nil then return nil end
  if key == "margin" then
    return normalizeMargin(val)
  elseif key == "stick" then
    return normalizeStick(val)
  end
  return val
end

-- =============================================================================
-- Reactive Props Proxy
-- =============================================================================

local PropsMethods = {}

local function getRuleComputed(props, key)
  local rc = props._ruleComputeds[key]
  if not rc then
    local widget = props._widget
    rc = R.computed(function()
      if widget.lwc and widget.lwc:has(key) then
        return normalizePropValue(key, widget.lwc:get(key))
      end
      return nil
    end)
    props._ruleComputeds[key] = rc
  end
  return rc
end

function PropsMethods:get(key)
  local comp = self._computeds[key]
  if comp then
    return comp:get()
  end

  local obs = self._observables[key]
  if obs then
    local val = obs:get()
    if val ~= nil then
      return val
    end
  end

  return getRuleComputed(self, key):get()
end

function PropsMethods:peek(key)
  local comp = self._computeds[key]
  if comp then
    return comp:peek()
  end

  local obs = self._observables[key]
  if obs then
    local val = obs:peek()
    if val ~= nil then
      return val
    end
  end

  return getRuleComputed(self, key):peek()
end

function PropsMethods:set(key, value)
  if self._computeds[key] then
    error("Cannot set computed property: " .. tostring(key))
  end

  local normVal = normalizePropValue(key, value)
  local obs = self._observables[key]
  if obs then
    obs:set(normVal)
  elseif normVal ~= nil then
    self._observables[key] = R.observable(normVal)
  end
end

function PropsMethods:computed(key, fn)
  self._computeds[key] = R.computed(function()
    return normalizePropValue(key, fn())
  end)
  return self
end

function PropsMethods:observable(key)
  local obs = self._observables[key]
  if not obs then
    local initial = self:peek(key)
    obs = R.observable(initial)
    self._observables[key] = obs
  end
  return obs
end

function PropsMethods:watch(key, fn)
  local widget = self._widget
  local watcher = R.watch(function()
    local val = self:get(key)
    fn(val, key, widget)
  end)
  table.insert(self._watchers, watcher)
  return watcher
end

function PropsMethods:watchAll(fn)
  local widget = self._widget
  local watcher = R.watch(function()
    local snapshot = {}
    for name, obs in pairs(self._observables) do
      local v = obs:get()
      if v ~= nil then snapshot[name] = v end
    end
    for name, comp in pairs(self._computeds) do
      snapshot[name] = comp:get()
    end
    fn(snapshot, widget)
  end)
  table.insert(self._watchers, watcher)
  return watcher
end

function PropsMethods:dispose()
  for _, watcher in ipairs(self._watchers) do
    watcher:dispose()
  end
  self._watchers = {}
end

function PropsMethods:getPropertyNames()
  local names = {}
  for name, obs in pairs(self._observables) do
    if obs:peek() ~= nil then
      names[name] = true
    end
  end
  for name in pairs(self._computeds) do
    names[name] = true
  end
  local result = {}
  for name in pairs(names) do
    table.insert(result, name)
  end
  return result
end

function PropsMethods:toTable()
  local result = {}
  for name, obs in pairs(self._observables) do
    local v = obs:peek()
    if v ~= nil then
      result[name] = v
    end
  end
  return result
end

local propsMT = {
  __index = function(t, key)
    local method = PropsMethods[key]
    if method then
      return method
    end
    return PropsMethods.get(t, key)
  end,
  __newindex = function(t, key, value)
    if type(key) == "string" and key:sub(1, 1) == "_" then
      rawset(t, key, value)
      return
    end
    PropsMethods.set(t, key, value)
  end
}

local function createPropsProxy(widget)
  local proxy = {
    _widget = widget,
    _observables = {},
    _computeds = {},
    _ruleComputeds = {},
    _watchers = {},
  }
  return setmetatable(proxy, propsMT)
end

-- =============================================================================
-- Widget Methods
-- =============================================================================

function Widget:add(kid)
  if not kid then return end
  table.insert(self.kids, kid)
  kid.parent = self
  if kid.lwc and self.lwc then
    kid.lwc.parent = self.lwc
  end
  return kid
end

local RESERVED_DEF_KEYS = {
  props = true,
  metrics = true,
  kids = true,
  tags = true,
  parent = true,
  id = true,
  name = true,
  layoutFunc = true,
  mouseAction = true,
  keyAction = true,
  textAction = true,
  eventRedirect = true,
  focused = true,
}

function Widget:init(def)
  ensureBaseDefaults()
  def = def or {}
  self.id = def.id or def.name or (def.props and def.props.id) or Widget.newID()
  self.name = self.id
  self.type = self.typeName or "Widget"
  self.parent = def.parent

  -- Collect automatic class hierarchy tags + id tag + explicit tags
  local initialTags = {}
  local seenTags = {}
  local function addInitTag(t)
    if t and not seenTags[t] then
      seenTags[t] = true
      table.insert(initialTags, t)
    end
  end

  local cls = getmetatable(self)
  while cls and cls.typeName do
    addInitTag("widget." .. cls.typeName)
    cls = cls.super
  end

  if self.id then
    addInitTag("id." .. self.id)
  end

  if def.tags then
    for _, t in ipairs(def.tags) do
      addInitTag(t)
    end
  end

  self.tags = initialTags
  self.lwc = setbox.newContext(initialTags, self.parent and self.parent.lwc)

  -- Create reactive props proxy
  self.props = createPropsProxy(self)
  self.metrics = self.props

  -- Initialize coordinates
  self.props:set("x", 0)
  self.props:set("y", 0)
  self.props:set("w", 0)
  self.props:set("h", 0)

  -- Merge top-level convenience fields into props (or self for function callbacks)
  for k, v in pairs(def) do
    if not RESERVED_DEF_KEYS[k] then
      if type(v) == "function" then
        self[k] = v
      else
        self.props:set(k, v)
      end
    end
  end

  -- Merge legacy def.metrics if supplied
  if type(def.metrics) == "table" then
    for k, v in pairs(def.metrics) do
      self.props:set(k, v)
    end
  end

  -- Merge def.props if supplied
  if type(def.props) == "table" then
    for k, v in pairs(def.props) do
      if k ~= "id" then
        self.props:set(k, v)
      end
    end
  end

  self.kids = {}
  if def.kids then
    for _, kid in ipairs(def.kids) do
      self:add(kid)
    end
  end

  self.layoutFunc = def.layoutFunc
  self.mouseAction = def.mouseAction
  self.keyAction = def.keyAction
  self.textAction = def.textAction
  self.eventRedirect = def.eventRedirect

  self.borderColor = def.borderColor
  self.backgroundColor = def.backgroundColor
  self.showBorder = def.showBorder
  self.showBackground = def.showBackground

  self.isMouseOver = false
  self.focused = false

  state.registerWidgetInstance(self)

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

-- Reactive Property & Tag Convenience Methods on Widget
function Widget:get(name)
  return self.props:get(name)
end

function Widget:set(name, value)
  self.props:set(name, value)
  return self
end

function Widget:computed(name, fn)
  self.props:computed(name, fn)
  return self
end

function Widget:observable(name)
  return self.props:observable(name)
end

function Widget:watch(name, fn)
  local watcher = R.watch(function()
    local val = self.props:get(name)
    fn(val, name, self)
  end)
  table.insert(self.props._watchers, watcher)
  return watcher
end

function Widget:watchAll(fn)
  local watcher = R.watch(function()
    local snapshot = {}
    for name, obs in pairs(self.props._observables) do
      local v = obs:get()
      if v ~= nil then snapshot[name] = v end
    end
    for name, comp in pairs(self.props._computeds) do
      snapshot[name] = comp:get()
    end
    fn(snapshot, self)
  end)
  table.insert(self.props._watchers, watcher)
  return watcher
end

function Widget:addTag(tag)
  if self.lwc then
    self.lwc:addTag(tag)
  end
  for _, t in ipairs(self.tags) do
    if t == tag then return self end
  end
  table.insert(self.tags, tag)
  return self
end

function Widget:removeTag(tag)
  if self.lwc then
    self.lwc:removeTag(tag)
  end
  for i, t in ipairs(self.tags) do
    if t == tag then
      table.remove(self.tags, i)
      break
    end
  end
  return self
end

function Widget:setTag(tag, active)
  if active then
    return self:addTag(tag)
  else
    return self:removeTag(tag)
  end
end

function Widget:toggleTag(tag)
  if self:hasTag(tag) then
    return self:removeTag(tag)
  else
    return self:addTag(tag)
  end
end

function Widget:hasTag(tag)
  if self.lwc then
    return self.lwc:hasTag(tag)
  end
  for _, t in ipairs(self.tags) do
    if t == tag then return true end
  end
  return false
end

function Widget:getPropertyNames()
  return self.props:getPropertyNames()
end

function Widget:toTable()
  local tagsList = {}
  if self.lwc then
    tagsList = self.lwc:getHierarchyTags()
  else
    for _, t in ipairs(self.tags) do
      table.insert(tagsList, t)
    end
  end
  return {
    id = self.id,
    tags = tagsList,
    props = self.props:toTable(),
  }
end

function Widget:dispose()
  if self.props then
    self.props:dispose()
  end
  state.unregisterWidgetInstance(self.id)
  for _, kid in ipairs(self.kids or {}) do
    if kid.dispose then kid:dispose() end
  end
end

function Widget:getMetrics()
  return self.props
end

function Widget:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 10 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 10 end
end

local function toColorTable(c, alpha)
  if not c then return nil end
  if type(c) == "table" then
    if c.toTable then
      local t = c:toTable()
      if alpha then t[4] = (t[4] or 1) * alpha end
      return t
    end
    return { c[1] or c.r or 1, c[2] or c.g or 1, c[3] or c.b or 1, (c[4] or c.a or 1) * (alpha or 1) }
  elseif type(c) == "string" then
    local r, g, b, a = state.hexToRgb(c)
    return { r, g, b, (a or 1) * (alpha or 1) }
  end
  return nil
end

-- Base rendering logic for a widget. Subclasses should override this
-- instead of draw() to ensure coordinate transformation is handled.
function Widget:drawSelf()
  local bg = self.backgroundColor or self.props.backgroundColor or self.props.background
  local border = self.borderColor or self.props.borderColor or self.props.border
  local showBg = (self.showBackground ~= nil) and self.showBackground or self.props.showBackground
  if showBg == nil then showBg = (bg ~= nil) end
  local showBorder = (self.showBorder ~= nil) and self.showBorder or self.props.showBorder
  if showBorder == nil then showBorder = (border ~= nil) end
  local alpha = self.props.opacity or 1.0

  if showBg and bg then
    local bgTbl = toColorTable(bg, alpha)
    if bgTbl then
      System.drawRect(0, 0, self.props.w or 0, self.props.h or 0, bgTbl)
    end
  end

  if (showBorder and border) or self.isMouseOver then
    local thickness = self.isMouseOver and 3 or (self.props.borderWidth or 1)
    local bTbl = toColorTable(border or DEFAULT_HIGHLIGHT_COLOR, alpha)
    if bTbl then
      System.drawRectLines(0, 0, self.props.w or 0, self.props.h or 0, thickness, bTbl)
    end
  end
end

-- Template method for drawing. Handles coordinate transformation and recursion.
function Widget:draw()
  if self.props.visible == false then return end

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
  -- x, y are LOCAL to the widget's origin
  return x >= 0 and x < (self.props.w or 0) and
         y >= 0 and y < (self.props.h or 0)
end

function Widget:setFocus()
  if focusedWidget and focusedWidget ~= self then
    focusedWidget.focused = false
    if focusedWidget.lwc then focusedWidget.lwc:removeTag("state.Focused") end
    if focusedWidget.onBlur then focusedWidget:onBlur() end
  end
  focusedWidget = self
  self.focused = true
  if self.lwc then self.lwc:addTag("state.Focused") end
  state.setFocus(self.id)
  if self.onFocus then self:onFocus() end
end

function Widget.getFocused() return focusedWidget end
function Widget.getHovered() return hoveredWidget end

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

  -- 1. Create a localized version of the event
  local localEvent = {}
  for k, v in pairs(event) do localEvent[k] = v end

  -- Store global coordinates
  localEvent.gx = event.x
  localEvent.gy = event.y

  if localEvent.x and localEvent.y then
    local ax, ay = self:getAbsolutePos()
    localEvent.x = localEvent.x - ax
    localEvent.y = localEvent.y - ay
  end

  -- 2. Call specialized handler (localized)
  local consumed = self:onEvent(localEvent)

  -- 3. Bubble if not consumed (using ORIGINAL global event coordinates for parent)
  if not consumed and self.parent then
    return self.parent:handleEvent(event)
  end
  return consumed
end

-- Subclasses should override this!
-- Receives event with LOCAL x, y and GLOBAL gx, gy
function Widget:onEvent(event)
  if event.type:match("^mouse") or event.type:match("^wheel") then
    if self.mouseAction then return self:mouseAction(event) end
  elseif event.type == "key" then
    if self.keyAction then return self:keyAction(event) end
  elseif event.type == "textInput" then
    if self.textAction then return self:textAction(event) end
  end
  return false
end

function Widget:hitTest(x, y)
  if self.props.visible == false then return nil end
  -- x, y are LOCAL to this widget's parent's content area
  -- 1. Is it inside me?
  if not self:contains(x, y) then return nil end

  -- 2. Check kids (localized to ME)
  local localX, localY = x, y
  for i = #self.kids, 1, -1 do
    local kid = self.kids[i]
    local hit = kid:hitTest(localX - (kid.props.x or 0), localY - (kid.props.y or 0))
    if hit then return hit end
  end

  return self
end

function Widget:findByID(id)
  if self.id == id then return self end
  for _, kid in ipairs(self.kids) do
    local found = kid:findByID(id)
    if found then return found end
  end
  return nil
end

function Widget:findByName(name)
  if self.name == name or self.id == name then return self end
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
    if hoveredWidget then
      hoveredWidget.isMouseOver = false
      if hoveredWidget.lwc then hoveredWidget.lwc:removeTag("state.Hovered") end
    end
    hoveredWidget = hit
    if hoveredWidget then
      hoveredWidget.isMouseOver = true
      if hoveredWidget.lwc then hoveredWidget.lwc:addTag("state.Hovered") end
      state.setHot(hoveredWidget.id)
    else
      state.setHot(nil)
    end
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
    local m = kid.props
    local margin = normalizeMargin(m.margin)
    local marginW = margin.left + margin.right
    local marginH = margin.top + margin.bottom
    local kw, kh = (m.prefW or 0) + marginW, (m.prefH or 0) + marginH
    if kw > maxW then maxW = kw end
    if kh > maxH then maxH = kh end
    sumW, sumH = sumW + kw, sumH + kh
  end

  local axis = self.layoutFunc and self.layoutFunc.axis
  if (self.props.prefW or 0) == 0 then
    if axis == "horizontal" then self.props.prefW = sumW else self.props.prefW = maxW end
  end
  if (self.props.prefH or 0) == 0 then
    if axis == "vertical" then self.props.prefH = sumH else self.props.prefH = maxH end
  end

  if (self.props.prefW or 0) == 0 then self.props.prefW = 10 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 10 end
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

-- --- Window Class ---
local Window = Widget.mkType("Window", Container)
function Window:init(def)
  def = def or {}
  Container.init(self, def)
  self.borderColor = def.borderColor or Color("#444")
  if type(self.borderColor) == "table" and self.borderColor.darken then
    self.backgroundColor = def.backgroundColor or self.borderColor:darken(0.8)
  else
    self.backgroundColor = def.backgroundColor or Color("#111")
  end
  if self.showBackground == nil then self.showBackground = true end
  if self.showBorder == nil then self.showBorder = true end
  self.props.w = def.width or def.w or (def.props and def.props.w) or 1280
  self.props.h = def.height or def.h or (def.props and def.props.h) or 720
end

function Window:layout(x, y, w, h)
  Container.layout(self, x, y, w, h)
end

function Window:calcMetrics() Container.calcMetrics(self) end
function Window:onResize(w, h) self.props.w, self.props.h = w, h end

Widget.Container = Container
Widget.Column = Column
Widget.Row = Row
Widget.Window = Window
Widget.Stick = Stick
Widget.batch = R.batch

function Widget.new(def)
  local instance = setmetatable({}, Widget)
  instance:init(def)
  return instance
end

-- Lazy-load Label and Button onto Widget when accessed to avoid circular require issues
setmetatable(Widget, {
  __index = function(t, k)
    if k == "Label" then
      local L = require("ui.Label")
      rawset(t, "Label", L)
      return L
    elseif k == "Button" then
      local B = require("ui.Button")
      rawset(t, "Button", B)
      return B
    end
    return nil
  end,
  __call = function(_, def)
    return Widget.new(def)
  end
})

return Widget
