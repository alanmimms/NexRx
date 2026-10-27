--[[
  GutterLeft.lua - Left-hand gutter for The River

  Renders carrier-aligned frequency tabs for active SignalBoxes,
  ambient alert ticks, and off-screen piles (▲ and ▼).
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")

local function clamp(v, minV, maxV)
  if v < minV then return minV end
  if v > maxV then return maxV end
  return v
end

local GutterLeft = Widget.mkType("GutterLeft", Widget)

setbox.rule {
  id = "gutter-left-defaults",
  tags = { "widget.GutterLeft" },
  priority = -1000,
  apply = {
    prefW = 68,
    background = "#070c16",
    border = "#131f33",
    tabBg = "#1e293b",
    tabBorder = "#38bdf8",
    tabText = "#f8fafc",
    alertColor = "#ef4444",
  }
}

function GutterLeft:init(def)
  Widget.init(self, def)
  self.activeBoxes = def.boxes or {}
  self.alerts = def.alerts or {}
end

function GutterLeft:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 68 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 300 end
end

function GutterLeft:drawSelf()
  local w = self.props.w or 0
  local h = self.props.h or 0
  if w <= 0 or h <= 0 then return end

  local fMin = self.props.freqMin or 14.0e6
  local fMax = self.props.freqMax or 14.35e6
  local span = math.max(1000, fMax - fMin)

  local bgR, bgG, bgB = state.hexToRgb(self.props.background or "#070c16")
  System.drawRect(0, 0, w, h, { bgR, bgG, bgB, 0.95 })
  System.drawRectLines(0, 0, w, h, 1, { 0.15, 0.2, 0.3, 0.8 })

  local topPileCount = 0
  local bottomPileCount = 0

  -- 1. Carrier-Aligned Tabs for SignalBoxes
  local boxes = self.activeBoxes or {}
  for _, box in ipairs(boxes) do
    local carrierFreq = box.carrierFreq or box.freq or (fMin + span * 0.5)
    if carrierFreq > fMax then
      topPileCount = topPileCount + 1
    elseif carrierFreq < fMin then
      bottomPileCount = bottomPileCount + 1
    else
      local norm = (carrierFreq - fMin) / span
      local cy = h - (norm * h)

      -- Draw tab: rounded rectangle with carrier frequency
      local tabW = w - 4
      local tabH = 18
      local tabX = 2
      local tabY = clamp(cy - tabH / 2, 2, h - tabH - 2)

      local tabBgR, tabBgG, tabBgB = state.hexToRgb(self.props.tabBg or "#1e293b")
      local tabBorderR, tabBorderG, tabBorderB = state.hexToRgb(self.props.tabBorder or "#38bdf8")

      System.drawRoundedRect(tabX, tabY, tabW, tabH, 3, { tabBgR, tabBgG, tabBgB, 0.9 })
      System.drawRectLines(tabX, tabY, tabW, tabH, 1, { tabBorderR, tabBorderG, tabBorderB, 0.8 })

      -- Carrier pointer triangle to the right edge
      System.drawLine(tabX + tabW, cy, w, cy, 2, { tabBorderR, tabBorderG, tabBorderB, 1.0 })

      local label = string.format("%.1f", carrierFreq / 1000.0)
      System.drawText(label, tabX + 3, tabY + 3, 11, { 0.95, 0.95, 1.0, 1.0 })
    end
  end

  -- 2. Ambient Alert Ticks
  for _, alert in ipairs(self.alerts or {}) do
    local f = alert.freq
    if f >= fMin and f <= fMax then
      local norm = (f - fMin) / span
      local ay = h - (norm * h)
      System.drawRect(w - 6, ay - 2, 5, 4, { 1.0, 0.2, 0.2, 0.9 })
    end
  end

  -- 3. Top and Bottom Off-Screen Piles
  if topPileCount > 0 then
    System.drawRoundedRect(2, 2, w - 4, 18, 3, { 0.2, 0.25, 0.35, 0.9 })
    System.drawText(string.format("▲ %d off", topPileCount), 6, 5, 11, { 1.0, 0.8, 0.3, 1.0 })
  end
  if bottomPileCount > 0 then
    System.drawRoundedRect(2, h - 20, w - 4, 18, 3, { 0.2, 0.25, 0.35, 0.9 })
    System.drawText(string.format("▼ %d off", bottomPileCount), 6, h - 17, 11, { 1.0, 0.8, 0.3, 1.0 })
  end
end

function GutterLeft:onEvent(event)
  if event.type == "mouseButton" and event.button == "LEFT" and event.isDown then
    local ey = event.y or 0
    if ey < 22 and self.onTopPileClick then
      self:onTopPileClick()
      return true
    elseif ey > (self.props.h or 300) - 22 and self.onBottomPileClick then
      self:onBottomPileClick()
      return true
    end
  end
  return false
end

return GutterLeft
