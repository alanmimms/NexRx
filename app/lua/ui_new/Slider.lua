local Widget = require("Widget")
local Color = require("Color")

local Slider = Widget.mkType("Slider", Widget)

function Slider:init(def)
  Widget.init(self, def)
  self.min = def.min or 0
  self.max = def.max or 100
  self.value = def.value or self.min
  self.onChanged = def.onChanged
  self.railH = 4
  self.isDragging = false
end

function Slider:calcMetrics()
  if self.metrics.prefW == 0 then self.metrics.prefW = 100 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 30 end
end

function Slider:setValue(v)
  local oldVal = self.value
  self.value = math.max(self.min, math.min(v, self.max))
  if self.value ~= oldVal and self.onChanged then
    self.onChanged(self, self.value)
  end
end

function Slider:updateValueFromPos(mouseX)
  local x, w = self.props.x, self.props.w
  local range = math.max(1, self.max - self.min)
  local percent = (mouseX - (x + 5)) / (w - 10)
  self:setValue(self.min + percent * range)
end

function Slider:handleEvent(event)
  if event.type == "mouseButton" then
    if event.button == 0 then -- Left Button
      if event.isDown then
        self.isDragging = true
        self:updateValueFromPos(event.x)
        return true
      else
        self.isDragging = false
        return true
      end
    end
  elseif event.type == "mouseMotion" and self.isDragging then
    self:updateValueFromPos(event.x)
    return true
  end
  return Widget.handleEvent(self, event)
end

function Slider:draw()
  Widget.draw(self)
  
  local x, y, w, h = self.props.x, self.props.y, self.props.w, self.props.h
  local trackY = y + (h - self.railH) / 2
  local handleW = 10
  local handleH = 16
  local handleY = y + (h - handleH) / 2
  
  -- Draw track
  System.drawRect(x + 5, trackY, w - 10, self.railH, Color("#888"):toTable())
  
  -- Calculate handle position
  local range = math.max(1, self.max - self.min)
  local percent = (self.value - self.min) / range
  local handleX = x + 5 + (w - 10 - handleW) * percent
  
  -- Draw handle
  System.drawRect(handleX, handleY, handleW, handleH, Color("#CCC"):toTable())
  System.drawRectLines(handleX, handleY, handleW, handleH, 1, Color("#FFF"):toTable())
end

local DiscreteSlider = Widget.mkType("DiscreteSlider", Slider)

function DiscreteSlider:init(def)
  local stops = def.stops or {}
  def.min = 0
  def.max = #stops > 1 and (#stops - 1) or 1
  def.value = def.initialIndex and (def.initialIndex - 1) or 0
  
  Slider.init(self, def)
  self.stops = stops
  self.currentIndex = (def.initialIndex or 1)
  self.labelPos = def.labelPos or "below" -- "above" or "below"
  self.fontSize = def.fontSize or 12
end

function DiscreteSlider:calcMetrics()
  local maxLabelW = 0
  for _, stop in ipairs(self.stops) do
    local tw = System.measureText(stop.label or "", self.fontSize)
    if tw > maxLabelW then maxLabelW = tw end
  end
  
  if self.metrics.prefW == 0 then
    self.metrics.prefW = math.max(100, #self.stops * (maxLabelW + 10))
  end
  if self.metrics.prefH == 0 then
    self.metrics.prefH = 40 -- increased default
  end
end

function DiscreteSlider:setValue(v)
  local rounded = math.floor(v + 0.5)
  local oldIndex = self.currentIndex
  
  Slider.setValue(self, rounded)
  self.currentIndex = math.floor(self.value + 1.5)
  if self.currentIndex > #self.stops then self.currentIndex = #self.stops end

  if self.currentIndex ~= oldIndex then
    local oldStop = self.stops[oldIndex]
    if oldStop and oldStop.onDeactivate then oldStop.onDeactivate(self, oldStop.value) end
    local newStop = self.stops[self.currentIndex]
    if newStop and newStop.onActivate then newStop.onActivate(self, newStop.value) end
  end
end

function DiscreteSlider:updateValueFromPos(mouseX)
  local x, w = self.props.x, self.props.w
  local nStops = #self.stops
  if nStops < 2 then
    self:setValue(0)
    return
  end

  local firstLabelW = System.measureText(self.stops[1].label or "", self.fontSize)
  local lastLabelW = System.measureText(self.stops[nStops].label or "", self.fontSize)
  
  local railStartX = x + firstLabelW / 2
  local railEndX = x + w - lastLabelW / 2
  local railWidth = railEndX - railStartX

  local percent = (mouseX - railStartX) / railWidth
  local range = self.max - self.min
  self:setValue(self.min + percent * range)
end

function DiscreteSlider:draw()
  Widget.draw(self) -- Draw background/border
  
  local x, y, w, h = self.props.x, self.props.y, self.props.w, self.props.h
  local nStops = #self.stops
  if nStops == 0 then return end
  
  local labelGap = 4
  local labelY = 0
  local railY = 0
  
  if self.labelPos == "below" then
    railY = y + (h - self.fontSize - labelGap) / 2
    labelY = railY + labelGap + 5 -- Adjust based on handle
  else
    labelY = y + 2
    railY = labelY + self.fontSize + labelGap + 8
  end

  -- Calculate rail span based on first and last label widths
  local firstLabelW = System.measureText(self.stops[1].label or "", self.fontSize)
  local lastLabelW = System.measureText(self.stops[nStops].label or "", self.fontSize)
  
  local railStartX = x + firstLabelW / 2
  local railEndX = x + w - lastLabelW / 2
  local railWidth = railEndX - railStartX
  
  -- Draw rail
  System.drawRect(railStartX, railY - self.railH/2, railWidth, self.railH, Color("#888"):toTable())
  
  -- Handle
  local range = math.max(1, self.max - self.min)
  local percent = (self.value - self.min) / range
  local handleW = 10
  local handleH = 16
  local handleX = railStartX + railWidth * percent - handleW / 2
  System.drawRect(handleX, railY - handleH/2, handleW, handleH, Color("#CCC"):toTable())
  System.drawRectLines(handleX, railY - handleH/2, handleW, handleH, 1, Color("#FFF"):toTable())

  -- Labels
  for i = 1, nStops do
    local p = (i - 1) / math.max(1, nStops - 1)
    local detentX = railStartX + railWidth * p
    local label = self.stops[i].label or tostring(i)
    local tw = System.measureText(label, self.fontSize)
    
    local lx = detentX - tw / 2
    if i == 1 then
      lx = x -- Left aligned
    elseif i == nStops then
      lx = x + w - tw -- Right aligned
    end

    local color = (i == self.currentIndex) and Color("#FFF") or Color("#888")
    System.drawText(label, lx, labelY, self.fontSize, color:toTable())
  end
end

Slider.DiscreteSlider = DiscreteSlider

return Slider
