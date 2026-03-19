local Widget = require("Widget")
local Color = require("Color")

local Waterfall = Widget.mkType("Waterfall", Widget)

function Waterfall:init(def)
  Widget.init(self, def)
  self.borderColor = def.borderColor or Color("#00F")
  self.backgroundColor = def.backgroundColor or Color("#000")
  self.showBackground = true
  self.showBorder = true
end

function Waterfall:calcMetrics()
  if self.metrics.prefW == 0 then self.metrics.prefW = 400 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 200 end
end

function Waterfall:draw()
  Widget.draw(self)
  
  local x, y, w, h = self.props.x, self.props.y, self.props.w, self.props.h
  local stripeH = 4
  local nStripes = math.floor((h - 10) / stripeH)
  for i = 1, nStripes do
    local sx = x + 5
    local sy = y + 5 + (i-1) * stripeH
    local sw = w - 10
    local sh = stripeH
    -- Random blueish/purplish color
    local r = 0.1 * math.random()
    local g = 0.1 * math.random()
    local b = 0.5 + 0.5 * math.random()
    local c = Color{r, g, b, 1.0}
    System.drawRect(sx, sy, sw, sh, c:toTable())
  end
end

return Waterfall
