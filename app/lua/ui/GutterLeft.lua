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

  -- 1. Carrier & Bandwidth Aligned Tabs for SignalBoxes
  local boxes = self.activeBoxes or {}
  for _, box in ipairs(boxes) do
    local carrierFreq = box.carrierFreq or box.freq or (fMin + span * 0.5)
    local bw = box.bandwidth or 2800
    local mode = box.mode or "USB"

    local lowFreq = (mode == "LSB") and (carrierFreq - bw) or carrierFreq
    local highFreq = (mode == "USB") and (carrierFreq + bw) or ((mode == "LSB") and carrierFreq or (carrierFreq + bw / 2))
    if mode == "CW" or mode == "AM" or mode == "FM" then
      lowFreq = carrierFreq - bw / 2
      highFreq = carrierFreq + bw / 2
    end

    if carrierFreq > fMax and highFreq > fMax then
      topPileCount = topPileCount + 1
    elseif carrierFreq < fMin and lowFreq < fMin then
      bottomPileCount = bottomPileCount + 1
    else
      local normCarrier = clamp((carrierFreq - fMin) / span, 0.0, 1.0)
      local normHigh = clamp((highFreq - fMin) / span, 0.0, 1.0)
      local normLow = clamp((lowFreq - fMin) / span, 0.0, 1.0)

      local cy = h - (normCarrier * h)
      local topY = h - (normHigh * h)
      local botY = h - (normLow * h)

      local tabBgR, tabBgG, tabBgB = state.hexToRgb(self.props.tabBg or "#1e293b")
      local tabBorderR, tabBorderG, tabBorderB = state.hexToRgb(self.props.tabBorder or "#38bdf8")

      -- Draw vertical passband skirt bracket along the right edge of GutterLeft
      System.drawRect(w - 3, topY, 3, math.max(2, botY - topY), { tabBorderR, tabBorderG, tabBorderB, 0.9 })
      System.drawLine(w - 7, topY, w, topY, 1.5, { tabBorderR, tabBorderG, tabBorderB, 0.95 })
      System.drawLine(w - 7, botY, w, botY, 1.5, { tabBorderR, tabBorderG, tabBorderB, 0.95 })

      -- Carrier pointer tick
      System.drawLine(w - 12, cy, w, cy, 2.0, { 1.0, 1.0, 1.0, 0.95 })

      local tabW = w - 6
      local tabX = 2
      local bwStr = string.format("%.1fk", bw / 1000.0)

      if mode == "USB" then
        -- Top edge useful frequency tag (highFreq), Carrier frequency tag (carrierFreq) & Bandwidth
        local topMhz = highFreq / 1000.0
        local carrierMhz = carrierFreq / 1000.0
        local tabH = 34
        local tabY = clamp(topY - 4, 2, h - tabH - 2)

        System.drawRoundedRect(tabX, tabY, tabW, tabH, 3, { tabBgR, tabBgG, tabBgB, 0.92 })
        System.drawRectLines(tabX, tabY, tabW, tabH, 1, { tabBorderR, tabBorderG, tabBorderB, 0.85 })

        -- Top edge frequency label (highFreq)
        local topLabel = string.format("%.1f", topMhz)
        System.drawText(topLabel, tabX + 3, tabY + 2, 10, { 0.7, 0.85, 1.0, 0.95 })

        -- Bandwidth label
        System.drawText(bwStr, tabX + tabW - (#bwStr * 6) - 3, tabY + 2, 9, { 0.95, 0.8, 0.25, 0.95 })

        -- Carrier frequency label at the bottom of the tab
        local cfLabel = string.format("%.1f", carrierMhz)
        System.drawText(cfLabel, tabX + 3, tabY + 16, 11, { 1.0, 1.0, 1.0, 1.0 })
      else
        local tabH = 22
        local tabY = clamp(cy - tabH / 2, 2, h - tabH - 2)
        System.drawRoundedRect(tabX, tabY, tabW, tabH, 3, { tabBgR, tabBgG, tabBgB, 0.92 })
        System.drawRectLines(tabX, tabY, tabW, tabH, 1, { tabBorderR, tabBorderG, tabBorderB, 0.85 })

        local cfLabel = string.format("%.1f", carrierFreq / 1000.0)
        System.drawText(cfLabel, tabX + 3, tabY + 3, 10, { 1.0, 1.0, 1.0, 1.0 })
        System.drawText(bwStr, tabX + tabW - (#bwStr * 6) - 3, tabY + 4, 9, { 0.95, 0.8, 0.25, 0.95 })
      end
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
  if event.type == "mouseWheel" then
    local river = self.parent and self.parent:findByName("id-river-view")
    if river and river.handleWheelZoom then
      return river:handleWheelZoom(event)
    end
  elseif event.type == "mouseButton" and event.button == "LEFT" and event.isDown then
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
