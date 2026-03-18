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
end

function Slider:calcMetrics(bridge)
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

function Slider:draw(bridge)
  Widget.draw(self, bridge)
  
  local x, y, w, h = self.props.x, self.props.y, self.props.w, self.props.h
  local trackY = y + (h - self.railH) / 2
  local handleW = 10
  local handleH = 16
  local handleY = y + (h - handleH) / 2
  
  -- Draw track
  bridge.drawRect(x + 5, trackY, w - 10, self.railH, Color("#888"):toTable())
  
  -- Calculate handle position
  local range = math.max(1, self.max - self.min)
  local percent = (self.value - self.min) / range
  local handleX = x + 5 + (w - 10 - handleW) * percent
  
  -- Draw handle
  bridge.drawRect(handleX, handleY, handleW, handleH, Color("#CCC"):toTable())
  bridge.drawRectLines(handleX, handleY, handleW, handleH, 1, Color("#FFF"):toTable())
end

local DiscreteSlider = Widget.mkType("DiscreteSlider", Slider)

function DiscreteSlider:init(def)
  local stops = def.stops or {}
  def.min = 0
  def.max = #stops > 1 and (#stops - 1) or 1
  def.value = def.initialIndex and (def.initialIndex - 1) or 0
  
  Slider.init(self, def)
  self.stops = stops
  self.currentIndex = def.initialIndex or 1
  self.labelPos = def.labelPos or "below" -- "above" or "below"
  self.fontSize = def.fontSize or 12
end

function DiscreteSlider:calcMetrics(bridge)
  local maxLabelW = 0
  for _, stop in ipairs(self.stops) do
    local tw = bridge.measureText(stop.label or "", self.fontSize)
    if tw > maxLabelW then maxLabelW = tw end
  end
  
  if self.metrics.prefW == 0 then
    self.metrics.prefW = math.max(100, #self.stops * (maxLabelW + 10))
  end
  if self.metrics.prefH == 0 then
    self.metrics.prefH = 40 -- increased default
  end
end

function DiscreteSlider:setIndex(idx)
  if idx < 1 or idx > #self.stops then return end
  local oldIndex = self.currentIndex
  if oldIndex == idx then return end
  
  local oldStop = self.stops[oldIndex]
  if oldStop and oldStop.onDeactivate then oldStop.onDeactivate(self, oldStop.value) end
  
  self.currentIndex = idx
  self:setValue(idx - 1)
  
  local newStop = self.stops[idx]
  if newStop and newStop.onActivate then newStop.onActivate(self, newStop.value) end
end

function DiscreteSlider:draw(bridge)
  Widget.draw(self, bridge) -- Draw background/border
  
  local x, y, w, h = self.props.x, self.props.y, self.props.w, self.props.h
  local nStops = #self.stops
  
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
  
  -- Draw rail
  bridge.drawRect(x + 5, railY - self.railH/2, w - 10, self.railH, Color("#888"):toTable())
  
  -- Handle
  local range = math.max(1, self.max - self.min)
  local percent = (self.value - self.min) / range
  local handleW = 10
  local handleH = 16
  local handleX = x + 5 + (w - 10 - handleW) * percent
  bridge.drawRect(handleX, railY - handleH/2, handleW, handleH, Color("#CCC"):toTable())
  bridge.drawRectLines(handleX, railY - handleH/2, handleW, handleH, 1, Color("#FFF"):toTable())

  -- Labels
  if nStops > 0 then
    local rangeW = w - 10
    for i = 1, nStops do
      local p = (i - 1) / math.max(1, nStops - 1)
      local stopX = x + 5 + rangeW * p
      local label = self.stops[i].label or tostring(i)
      local tw = bridge.measureText(label, self.fontSize)
      
      local color = (i == self.currentIndex) and Color("#FFF") or Color("#888")
      bridge.drawText(label, stopX - tw/2, labelY, self.fontSize, color:toTable())
    end
  end
end

Slider.DiscreteSlider = DiscreteSlider

return Slider
