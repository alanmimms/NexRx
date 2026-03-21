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
end

return Waterfall
