local Widget = require("ui.Widget")
local state = require("ui.State")
local Color = require("ui.Color")
local Model = require("Model")
local Hardware = require("Hardware")
local events = require("Events")

local Waterfall = Widget.mkType("Waterfall", Widget)

function Waterfall:init(def)
  Widget.init(self, def)
  self.dragging = false
end

function Waterfall:calcMetrics()
  if self.metrics.prefW == 0 then self.metrics.prefW = 400 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 200 end
end

function Waterfall:getFreqAtPx(px)
  local w = self.props.w
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = (_G.sampleRate or 384000) / zoom
  local center = Model.spectrumCenterFreq:peek()
  return center + (px / w - 0.5) * span
end

function Waterfall:onEvent(event)
  if event.type == "mouseWheel" then
    local step = 100
    if isCtrlDown and isCtrlDown() then step = 10000
    elseif isShiftDown and isShiftDown() then step = 100000 end
    local current = Model.rx.VFO.activeValue:get()
    Model.set("rx.VFO.activeValue", current + event.delta * step)
    return true
  elseif event.type == "mouseButton" and event.button == "LEFT" then
    if event.isDown then
      self.dragging = true
      local freq = self:getFreqAtPx(event.x)
      Model.set("rx.VFO.activeValue", freq)
      state.setActive(self.id)
      return true
    else
      self.dragging = false
      state.setActive(nil)
      return true
    end
  elseif event.type == "mouseMotion" and self.dragging then
    local freq = self:getFreqAtPx(event.x)
    Model.set("rx.VFO.activeValue", freq)
    return true
  end
  return Widget.onEvent(self, event)
end

function Waterfall:drawSelf()
  local w, h = self.props.w, self.props.h
  
  -- Local 0,0
  Hardware.renderWaterfall(0, 0, w, h, Model.waterfall.zoom:get(), 0.5)
end

return Waterfall
