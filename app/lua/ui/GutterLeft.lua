--[[
  GutterLeft.lua - Left-hand gutter for The River

  Renders carrier-aligned frequency tabs for active SignalBoxes,
  ambient alert ticks, and off-screen piles (▲ and ▼).
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")
local Model = require("Model")

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
    prefW = 76,
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
  self.topPileBoxes = {}
  self.bottomPileBoxes = {}
  self.topPileAlerts = {}
  self.bottomPileAlerts = {}
  self.hoveredPile = nil
end

function GutterLeft:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 76 end
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

  local rulerH = self.props.rulerH or 22
  local canvasH = math.max(10, h - rulerH)

  self.topPileBoxes = {}
  self.bottomPileBoxes = {}
  self.topPileAlerts = {}
  self.bottomPileAlerts = {}

  local topPileCount = 0
  local bottomPileCount = 0

  -- 1. Carrier & Bandwidth Aligned Tabs for SignalBoxes
  local boxes = self.activeBoxes or {}
  self.tabHitBoxes = {}
  for i, box in ipairs(boxes) do
    local carrierFreq = box.carrierFreq or box.freq or (fMin + span * 0.5)
    local bw = box.bandwidth or 2800
    local mode = box.mode or "USB"
    local isSelected = (Model.selectedSignalBoxIndex:get() == (box.boxIndex or i))

    local lowFreq = (mode == "LSB") and (carrierFreq - bw) or carrierFreq
    local highFreq = (mode == "USB") and (carrierFreq + bw) or ((mode == "LSB") and carrierFreq or (carrierFreq + bw / 2))
    if mode == "CW" or mode == "AM" or mode == "FM" then
      lowFreq = carrierFreq - bw / 2
      highFreq = carrierFreq + bw / 2
    end

    if carrierFreq > fMax and highFreq > fMax then
      topPileCount = topPileCount + 1
      table.insert(self.topPileBoxes, box)
    elseif carrierFreq < fMin and lowFreq < fMin then
      bottomPileCount = bottomPileCount + 1
      table.insert(self.bottomPileBoxes, box)
    else
      local normCarrier = clamp((carrierFreq - fMin) / span, 0.0, 1.0)
      local normHigh = clamp((highFreq - fMin) / span, 0.0, 1.0)
      local normLow = clamp((lowFreq - fMin) / span, 0.0, 1.0)

      local cy = rulerH + canvasH - (normCarrier * canvasH)
      local topY = rulerH + canvasH - (normHigh * canvasH)
      local botY = rulerH + canvasH - (normLow * canvasH)

      local tabBgR, tabBgG, tabBgB = state.hexToRgb(self.props.tabBg or "#1e293b")
      local tabBorderR, tabBorderB, tabBorderG
      if isSelected then
        tabBorderR, tabBorderG, tabBorderB = 0.98, 0.88, 0.28
      else
        local tr, tg, tb = state.hexToRgb(self.props.tabBorder or "#38bdf8")
        tabBorderR, tabBorderG, tabBorderB = tr, tg, tb
      end

      -- Draw vertical passband skirt bracket along the right edge of GutterLeft
      System.drawRect(w - 3, topY, 3, math.max(2, botY - topY), { tabBorderR, tabBorderG, tabBorderB, 0.9 })
      System.drawLine(w - 7, topY, w, topY, 1.5, { tabBorderR, tabBorderG, tabBorderB, 0.95 })
      System.drawLine(w - 7, botY, w, botY, 1.5, { tabBorderR, tabBorderG, tabBorderB, 0.95 })

      local tabW = w - 8
      local tabX = 2
      local bwStr = (bw < 1000) and string.format("%dHz", bw) or string.format("%.1fk", bw / 1000.0)
      local cfLabel = string.format("%.3f", carrierFreq / 1e6)
      local highLabel = string.format("%.3f", highFreq / 1e6)
      local lowLabel = string.format("%.3f", lowFreq / 1e6)
      local row2Text = string.format("%s %s", bwStr, mode)

      local carrierTabH = 28
      local carrierTabY
      local targetLineY

      if mode == "USB" then
        targetLineY = botY
      elseif mode == "LSB" then
        targetLineY = topY
      else
        targetLineY = cy
      end

      carrierTabY = math.floor(targetLineY - carrierTabH / 2 + 0.5)
      carrierTabY = clamp(carrierTabY, rulerH + 2, h - carrierTabH - 2)
      table.insert(self.tabHitBoxes, { y = carrierTabY, h = carrierTabH, index = box.boxIndex or i })

      -- Carrier tag with 2 rows of text: frequency on top, bandwidth below
      System.drawRoundedRect(tabX, carrierTabY, tabW, carrierTabH, 3, { tabBgR, tabBgG, tabBgB, 0.94 })
      System.drawRectLines(tabX, carrierTabY, tabW, carrierTabH, isSelected and 2 or 1, { tabBorderR, tabBorderG, tabBorderB, isSelected and 0.95 or 0.75 })

      -- Row 1 (top): Frequency
      System.drawText(cfLabel, tabX + 4, carrierTabY + 2, 11, { 1.0, 1.0, 1.0, 1.0 })
      -- Row 2 (bottom): Bandwidth & Mode
      System.drawText(row2Text, tabX + 4, carrierTabY + 14, 10, { 0.55, 0.85, 1.0, 0.95 })
      -- Pointer tick to the exact carrier frequency on the River
      System.drawLine(tabX + tabW, targetLineY, w, targetLineY, 2.0, { 1.0, 1.0, 1.0, 0.95 })

      -- Secondary boundary tag at the other skirt edge if space allows
      if mode == "USB" and (botY - topY) >= 36 then
        local topTabH = 16
        local topTabY = math.floor(topY - topTabH / 2 + 0.5)
        topTabY = clamp(topTabY, rulerH + 2, h - topTabH - 2)
        if topTabY + topTabH < carrierTabY - 2 then
          table.insert(self.tabHitBoxes, { y = topTabY, h = topTabH, index = box.boxIndex or i })
          System.drawRoundedRect(tabX + 4, topTabY, tabW - 4, topTabH, 2, { tabBgR, tabBgG, tabBgB, 0.85 })
          System.drawRectLines(tabX + 4, topTabY, tabW - 4, topTabH, 1, { tabBorderR, tabBorderG, tabBorderB, 0.6 })
          System.drawText(highLabel, tabX + 6, math.floor(topY - 4), 9, { 0.7, 0.85, 1.0, 0.9 })
          System.drawLine(tabX + tabW, topY, w, topY, 1.5, { tabBorderR, tabBorderG, tabBorderB, 0.95 })
        end
      elseif mode == "LSB" and (botY - topY) >= 36 then
        local botTabH = 16
        local botTabY = math.floor(botY - botTabH / 2 + 0.5)
        botTabY = clamp(botTabY, rulerH + 2, h - botTabH - 2)
        if botTabY > carrierTabY + carrierTabH + 2 then
          table.insert(self.tabHitBoxes, { y = botTabY, h = botTabH, index = box.boxIndex or i })
          System.drawRoundedRect(tabX + 4, botTabY, tabW - 4, botTabH, 2, { tabBgR, tabBgG, tabBgB, 0.85 })
          System.drawRectLines(tabX + 4, botTabY, tabW - 4, botTabH, 1, { tabBorderR, tabBorderG, tabBorderB, 0.6 })
          System.drawText(lowLabel, tabX + 6, math.floor(botY - 4), 9, { 0.7, 0.85, 1.0, 0.9 })
          System.drawLine(tabX + tabW, botY, w, botY, 1.5, { tabBorderR, tabBorderG, tabBorderB, 0.95 })
        end
      end
    end
  end

  -- 2. Ambient Alert Ticks
  for _, alert in ipairs(self.alerts or {}) do
    local f = alert.freq or 0
    if f > fMax then
      topPileCount = topPileCount + 1
      table.insert(self.topPileAlerts, alert)
    elseif f < fMin then
      bottomPileCount = bottomPileCount + 1
      table.insert(self.bottomPileAlerts, alert)
    else
      local norm = (f - fMin) / span
      local ay = rulerH + canvasH - (norm * canvasH)
      System.drawRect(w - 6, ay - 2, 5, 4, { 1.0, 0.2, 0.2, 0.9 })
    end
  end

  -- 3. Top and Bottom Off-Screen Piles
  if topPileCount > 0 then
    local isHov = (self.hoveredPile == "top")
    local pillBg = isHov and { 0.28, 0.35, 0.48, 0.95 } or { 0.2, 0.25, 0.35, 0.9 }
    System.drawRoundedRect(2, 2, w - 4, 18, 3, pillBg)
    System.drawRectLines(2, 2, w - 4, 18, 1, isHov and { 0.95, 0.8, 0.3, 0.9 } or { 0.4, 0.5, 0.65, 0.7 })
    System.drawText(string.format("▲ %d off", topPileCount), 6, 5, 11, { 1.0, 0.85, 0.35, 1.0 })

    -- Hover Tooltip Card below top pill
    if isHov then
      local cardW = 160
      local items = {}
      for _, b in ipairs(self.topPileBoxes) do
        table.insert(items, string.format("%.3f %s %s", (b.carrierFreq or 0) / 1e6, b.mode or "", b.label or ""))
      end
      for _, a in ipairs(self.topPileAlerts) do
        table.insert(items, string.format("%.3f %s", (a.freq or 0) / 1e6, a.label or "Alert"))
      end
      local nShow = math.min(#items, 4)
      local cardH = 18 + nShow * 14 + 6
      System.drawRoundedRect(2, 22, cardW, cardH, 4, { 0.08, 0.12, 0.22, 0.95 })
      System.drawRectLines(2, 22, cardW, cardH, 1, { 0.35, 0.5, 0.75, 0.9 })
      System.drawText(string.format("▲ %d ABOVE VISIBLE", topPileCount), 6, 26, 10, { 0.95, 0.8, 0.3, 1.0 })
      for j = 1, nShow do
        System.drawText(items[j], 8, 26 + j * 14, 10, { 0.9, 0.95, 1.0, 0.95 })
      end
    end
  end

  if bottomPileCount > 0 then
    local isHov = (self.hoveredPile == "bottom")
    local pillBg = isHov and { 0.28, 0.35, 0.48, 0.95 } or { 0.2, 0.25, 0.35, 0.9 }
    System.drawRoundedRect(2, h - 20, w - 4, 18, 3, pillBg)
    System.drawRectLines(2, h - 20, w - 4, 18, 1, isHov and { 0.95, 0.8, 0.3, 0.9 } or { 0.4, 0.5, 0.65, 0.7 })
    System.drawText(string.format("▼ %d off", bottomPileCount), 6, h - 17, 11, { 1.0, 0.85, 0.35, 1.0 })

    -- Hover Tooltip Card above bottom pill
    if isHov then
      local cardW = 160
      local items = {}
      for _, b in ipairs(self.bottomPileBoxes) do
        table.insert(items, string.format("%.3f %s %s", (b.carrierFreq or 0) / 1e6, b.mode or "", b.label or ""))
      end
      for _, a in ipairs(self.bottomPileAlerts) do
        table.insert(items, string.format("%.3f %s", (a.freq or 0) / 1e6, a.label or "Alert"))
      end
      local nShow = math.min(#items, 4)
      local cardH = 18 + nShow * 14 + 6
      local cardY = h - 22 - cardH
      System.drawRoundedRect(2, cardY, cardW, cardH, 4, { 0.08, 0.12, 0.22, 0.95 })
      System.drawRectLines(2, cardY, cardW, cardH, 1, { 0.35, 0.5, 0.75, 0.9 })
      System.drawText(string.format("▼ %d BELOW VISIBLE", bottomPileCount), 6, cardY + 4, 10, { 0.95, 0.8, 0.3, 1.0 })
      for j = 1, nShow do
        System.drawText(items[j], 8, cardY + 4 + j * 14, 10, { 0.9, 0.95, 1.0, 0.95 })
      end
    end
  end
end

function GutterLeft:onTopPileClick()
  local fMax = self.props.freqMax or 14.35e6
  local targetFreq = nil
  local minAbove = math.huge
  local boxes = (#self.topPileBoxes > 0) and self.topPileBoxes or (self.activeBoxes or {})
  for _, box in ipairs(boxes) do
    local f = box.carrierFreq or box.freq or 0
    if f > fMax and f < minAbove then minAbove = f; targetFreq = f end
  end
  local alerts = (#self.topPileAlerts > 0) and self.topPileAlerts or (self.alerts or {})
  for _, a in ipairs(alerts) do
    local f = a.freq or 0
    if f > fMax and f < minAbove then minAbove = f; targetFreq = f end
  end
  if targetFreq and self.onCenterFrequency then
    self:onCenterFrequency(targetFreq)
  end
end

function GutterLeft:onBottomPileClick()
  local fMin = self.props.freqMin or 14.0e6
  local targetFreq = nil
  local maxBelow = -math.huge
  local boxes = (#self.bottomPileBoxes > 0) and self.bottomPileBoxes or (self.activeBoxes or {})
  for _, box in ipairs(boxes) do
    local f = box.carrierFreq or box.freq or 0
    if f < fMin and f > maxBelow then maxBelow = f; targetFreq = f end
  end
  local alerts = (#self.bottomPileAlerts > 0) and self.bottomPileAlerts or (self.alerts or {})
  for _, a in ipairs(alerts) do
    local f = a.freq or 0
    if f < fMin and f > maxBelow then maxBelow = f; targetFreq = f end
  end
  if targetFreq and self.onCenterFrequency then
    self:onCenterFrequency(targetFreq)
  end
end

function GutterLeft:onEvent(event)
  if event.type == "mouseWheel" then
    local river = self.parent and self.parent:findByName("id-river-view")
    if river and river.handleWheelZoom then
      return river:handleWheelZoom(event)
    end
  elseif event.type == "mouseMotion" then
    local ey = event.y or 0
    local h = self.props.h or 300
    if ey <= 22 and (#self.topPileBoxes + #self.topPileAlerts > 0) then
      self.hoveredPile = "top"
    elseif ey >= h - 22 and (#self.bottomPileBoxes + #self.bottomPileAlerts > 0) then
      self.hoveredPile = "bottom"
    else
      self.hoveredPile = nil
    end
    return false
  elseif event.type == "mouseButton" and event.button == "LEFT" and event.isDown then
    local ey = event.y or 0
    if ey < 22 and self.onTopPileClick then
      self:onTopPileClick()
      return true
    elseif ey > (self.props.h or 300) - 22 and self.onBottomPileClick then
      self:onBottomPileClick()
      return true
    else
      for _, hit in ipairs(self.tabHitBoxes or {}) do
        if ey >= hit.y and ey <= hit.y + hit.h then
          Model.selectedSignalBoxIndex:set(hit.index)
          if self.onSelectBox then self:onSelectBox(hit.index) end
          return true
        end
      end
    end
  end
  return false
end

return GutterLeft
