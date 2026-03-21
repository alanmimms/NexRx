local Widget = require("ui.Widget")
local state = require("ui.State")
local Color = require("ui.Color")
local Model = require("Model")
local Hardware = require("Hardware")
local events = require("Events")

local Waterfall = Widget.mkType("Waterfall", Widget)

function Waterfall:init(def)
  Widget.init(self, def)
end

function Waterfall:calcMetrics()
  if self.metrics.prefW == 0 then self.metrics.prefW = 400 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 200 end
end

function Waterfall:drawSelf()
  local w, h = self.props.w, self.props.h
  
  -- Local 0,0
  Hardware.renderWaterfall(0, 0, w, h, Model.waterfall.zoom:get(), 0.5)
  
  -- Render SignalBox highlights on waterfall too
  local boxes = Model.signalBoxes:get()
  local selectedIdx = Model.selectedSignalBoxIndex:get()
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = _G.sampleRate / zoom
  local center = Model.spectrumCenterFreq:peek()

  for i, box in ipairs(boxes) do
     local bw = box.bandwidth
     local freq = box.frequency
     local boxW = (bw / span) * w
     local boxX = (w / 2) + ((freq - center) / span) * w - (boxW / 2)
     
     local alpha = (i == selectedIdx) and 0.2 or 0.1
     local r, g, b = 1.0, 1.0, 0.0 -- Yellow
     System.drawRect(boxX, 0, boxW, h, {r, g, b, alpha})
  end
end

return Waterfall
