local Widget = require("ui.Widget")
local state = require("ui.State")
local Model = require("Model")
local Hardware = require("Hardware")

local Waterfall = Widget.mkType("Waterfall", Widget)

function Waterfall:init(def)
  Widget.init(self, def)
  self.dragging = false
end

function Waterfall:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 400 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 200 end
end

function Waterfall:getFreqAtPx(px)
  local w = math.max(1, self.props.w or 1)
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
    Model.set("rx.VFO.activeValue", current + (event.delta or 0) * step)
    return true
  elseif event.type == "mouseButton" and event.button == "LEFT" then
    if event.isDown then
      self.dragging = true
      local freq = self:getFreqAtPx(event.x)
      Model.set("rx.VFO.activeValue", freq)
      state.setActive(self.id)
      if self.lwc then self.lwc:addTag("state.Active") end
      return true
    else
      self.dragging = false
      state.setActive(nil)
      if self.lwc then self.lwc:removeTag("state.Active") end
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
  local w, h = self.props.w or 0, self.props.h or 0

  -- Local 0,0
  Hardware.renderWaterfall(0, 0, w, h, Model.waterfall.zoom:get(), 0.5)
end

return Waterfall
