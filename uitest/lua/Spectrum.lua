local Widget = require("Widget")
local Color = require("Color")

local Spectrum = Widget.mkType("Spectrum", Widget)

function Spectrum:init(def)
  Widget.init(self, def)
  self.borderColor = def.borderColor or Color("#0F0")
  self.backgroundColor = def.backgroundColor or Color("#111")
  self.showBackground = true
  self.showBorder = true
end

function Spectrum:calcMetrics(bridge)
  if self.metrics.prefW == 0 then self.metrics.prefW = 400 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 200 end
end

function Spectrum:draw(bridge)
  Widget.draw(self, bridge)
  
  local x, y, w, h = self.props.x, self.props.y, self.props.w, self.props.h
  local startX = x + 5
  local startY = y + h - 5
  local endX = x + w - 5
  
  -- Draw some fake spectrum peaks
  local points = {}
  local nPoints = 80
  for i = 0, nPoints do
    local px = startX + (endX - startX) * (i / nPoints)
    -- Ensure random range is valid (upper bound >= lower bound)
    local maxPeak = math.max(5, h - 10)
    local py = startY - math.random(5, maxPeak)
    table.insert(points, {px, py})
  end
  
  for i = 1, #points - 1 do
    bridge.drawLine(points[i][1], points[i][2], points[i+1][1], points[i+1][2], 2, self.borderColor:toTable())
  end
end

return Spectrum
