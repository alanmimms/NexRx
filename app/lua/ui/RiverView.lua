--[[
  RiverView.lua - The River primary viewport and spectral workspace

  Implements the split-river canvas, top time ruler, CAD discontinuity
  break line, live-pinned region, and 2D time-frequency navigation.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")
local Model = require("Model")
local SignalBox = require("ui.SignalBox")
local RiverBehaviors = require("ui.RiverBehaviors")

local function clamp(v, minV, maxV)
  if v < minV then return minV end
  if v > maxV then return maxV end
  return v
end

local RiverView = Widget.mkType("RiverView", Widget.Container)

setbox.rule {
  id = "riverview-defaults",
  tags = { "widget.RiverView" },
  priority = -1000,
  apply = {
    background = "#050811",
    border = "#1e293b",
    rulerBg = "#0a0f1d",
    rulerText = "#94a3b8",
    cadBreakColor = { 0.75, 0.8, 0.9, 0.75 },
    cadDimensionColor = { 0.95, 0.8, 0.25, 0.9 },
    gridColor = { 0.15, 0.2, 0.3, 0.0 }, -- alpha 0 for zero-cost default
    pinnedSliverW = 60,
    rulerH = 22,
  }
}

function RiverView:init(def)
  def = def or {}
  Widget.Container.init(self, def)

  self.timeSpanSec = def.timeSpan or 30.0
  self.timeOffsetSec = def.timeOffset or 0.0
  self.totalBufferSec = 60.0

  self.freqMin = def.freqMin or 14.0e6
  self.freqMax = def.freqMax or 14.35e6
  self.hwCenterFreq = def.centerFreq or 14.175e6
  self.hwBandwidth = def.bandwidth or 350e3

  self.isNavigating = false
  self.dragStartX = 0
  self.dragStartY = 0
  self.dragStartTimeOffset = 0
  self.dragStartCenterFreq = 0

  self.signalBoxes = {}
  if def.signalBoxes then
    for _, boxDef in ipairs(def.signalBoxes) do
      self:addSignalBox(boxDef)
    end
  end
end

function RiverView:addSignalBox(boxDef)
  local box = SignalBox(boxDef)
  table.insert(self.signalBoxes, box)
  self:add(box)
  return box
end

function RiverView:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 800 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 450 end
  for _, kid in ipairs(self.kids or {}) do
    kid:calcMetrics()
  end
end

function RiverView:getNormalizedFreqRange()
  local hwMin = self.hwCenterFreq - self.hwBandwidth / 2
  local hwMax = self.hwCenterFreq + self.hwBandwidth / 2
  local hwSpan = math.max(1, hwMax - hwMin)

  local fMinNorm = clamp((self.freqMin - hwMin) / hwSpan, 0.0, 1.0)
  local fMaxNorm = clamp((self.freqMax - hwMin) / hwSpan, 0.0, 1.0)
  if fMaxNorm <= fMinNorm then fMaxNorm = fMinNorm + 0.001 end
  return fMinNorm, fMaxNorm
end

function RiverView:layout(x, y, w, h)
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  local rulerH = self.props.rulerH or 22
  local canvasH = math.max(10, h - rulerH)
  local fSpan = math.max(1000, self.freqMax - self.freqMin)

  -- Position SignalBoxes across the canvas
  for _, box in ipairs(self.signalBoxes or {}) do
    local bw = box.bandwidth or 2800
    local cFreq = box.carrierFreq or ((self.freqMin + self.freqMax) / 2)

    local lowFreq = (box.mode == "LSB") and (cFreq - bw) or cFreq
    local highFreq = (box.mode == "USB") and (cFreq + bw) or ((box.mode == "LSB") and cFreq or (cFreq + bw / 2))
    if box.mode == "CW" or box.mode == "AM" then
      lowFreq = cFreq - bw / 2
      highFreq = cFreq + bw / 2
    end

    local normLow = (lowFreq - self.freqMin) / fSpan
    local normHigh = (highFreq - self.freqMin) / fSpan

    local boxY = rulerH + canvasH - (normHigh * canvasH)
    local boxH = math.max(12, (normHigh - normLow) * canvasH)

    box:layout(0, boxY, w, boxH)
  end
end

function RiverView:drawSelf()
  local w = self.props.w or 0
  local h = self.props.h or 0
  if w <= 0 or h <= 0 then return end

  local rulerH = self.props.rulerH or 22
  local canvasY = rulerH
  local canvasH = math.max(10, h - rulerH)

  local fMinNorm, fMaxNorm = self:getNormalizedFreqRange()
  local riverEngine = _G.river or _G.waterfall
  local ox, oy = state.getOffset()

  local pinnedW = self.props.pinnedSliverW or 60
  local isSplit = (self.timeOffsetSec > 0.01)

  -- 1. Main River Texture Pass
  if riverEngine and riverEngine.renderRiver then
    if not isSplit then
      -- Continuous single River canvas flush to NOW line
      riverEngine.renderRiver(ox, oy + canvasY, w, canvasH,
                              1.0, 0.0, fMinNorm, fMaxNorm)
    else
      -- Split River: History Region + CAD Break Gap + Live Pinned Sliver
      local breakW = 24
      local historyW = math.max(20, w - breakW - pinnedW)
      local histOffsetNorm = clamp(self.timeOffsetSec / self.totalBufferSec, 0.0, 0.8)
      local histSpanNorm = clamp(self.timeSpanSec / self.totalBufferSec, 0.1, 1.0 - histOffsetNorm)

      -- Left: Scrolled History Region
      riverEngine.renderRiver(ox, oy + canvasY, historyW, canvasH,
                              histSpanNorm, histOffsetNorm, fMinNorm, fMaxNorm)

      -- Right: Real-time Live Pinned Region (1-2s sliver)
      local liveSpanNorm = clamp(2.0 / self.totalBufferSec, 0.02, 0.1)
      riverEngine.renderRiver(ox + historyW + breakW, oy + canvasY, pinnedW, canvasH,
                              liveSpanNorm, 0.0, fMinNorm, fMaxNorm)

      -- Middle: CAD-style Discontinuity Break Line
      local bx = historyW + breakW / 2
      local cadColor = self.props.cadBreakColor or { 0.75, 0.8, 0.9, 0.75 }
      local zigzagStep = 18
      local zigzagAmp = 6
      local curY = canvasY
      local sign = 1

      while curY < canvasY + canvasH do
        local nextY = math.min(canvasY + canvasH, curY + zigzagStep)
        local curX = bx + (sign * zigzagAmp)
        local nextX = bx - (sign * zigzagAmp)
        System.drawLine(curX, curY, nextX, nextY, 1.5, cadColor)
        curY = nextY
        sign = -sign
      end

      -- Dimension line across CAD gap in Top Ruler
      local gapSec = math.floor(self.timeOffsetSec + 0.5)
      local dimText = string.format("|<-- %ds -->|", gapSec)
      local dimColor = self.props.cadDimensionColor or { 0.95, 0.8, 0.25, 0.9 }
      local dw = System.measureText(dimText, 11)
      System.drawText(dimText, bx - dw / 2, 4, 11, dimColor)
    end
  end

  -- 2. Optional CAD Grid Lines (zero cost if alpha is 0)
  local gridColor = self.props.gridColor
  if gridColor and (gridColor[4] or 0) > 0 then
    local gridDivs = 4
    for i = 1, gridDivs - 1 do
      local gy = canvasY + (i / gridDivs) * canvasH
      System.drawLine(0, gy, w, gy, 1.0, gridColor)
    end
  end

  -- 3. Top Time Ruler
  local rBgR, rBgG, rBgB = state.hexToRgb(self.props.rulerBg or "#0a0f1d")
  System.drawRect(0, 0, w, rulerH, { rBgR, rBgG, rBgB, 0.95 })
  System.drawLine(0, rulerH, w, rulerH, 1.0, { 0.2, 0.25, 0.35, 0.8 })

  if not isSplit then
    -- Live time markers: e.g. -30s, -20s, -10s, NOW
    local marks = { { 0.05, "-30s" }, { 0.35, "-20s" }, { 0.70, "-10s" }, { 0.96, "NOW" } }
    for _, m in ipairs(marks) do
      local tx = m[1] * w
      System.drawLine(tx, rulerH - 5, tx, rulerH, 1.0, { 0.5, 0.55, 0.65, 0.8 })
      System.drawText(m[2], tx - 10, 4, 11, { 0.75, 0.8, 0.85, 0.9 })
    end
  else
    -- Scrolled historical markers on left, NOW on pinned right
    local hSecEnd = self.timeOffsetSec
    local hSecStart = self.timeOffsetSec + self.timeSpanSec
    local hText = string.format("-%ds ... -%ds", math.floor(hSecStart), math.floor(hSecEnd))
    System.drawText(hText, 10, 4, 11, { 0.85, 0.85, 0.9, 0.95 })
    System.drawText("NOW", w - pinnedW + 4, 4, 11, { 0.2, 0.95, 0.3, 0.95 })
  end
end

function RiverView:onEvent(event)
  if event.type == "mouseButton" and event.button == "LEFT" then
    if event.isDown then
      local rx = event.x or 0
      local w = self.props.w or 800
      local pinnedW = self.props.pinnedSliverW or 60

      -- Clicking in live pinned sliver snaps back to real time
      if self.timeOffsetSec > 0 and rx >= w - pinnedW then
        self.timeOffsetSec = 0.0
        return true
      end

      self.isNavigating = true
      self.dragStartX = event.gx or event.x or 0
      self.dragStartY = event.gy or event.y or 0
      self.dragStartTimeOffset = self.timeOffsetSec
      self.dragStartCenterFreq = (self.freqMin + self.freqMax) / 2
      state.setActive(self.id)
      return true
    else
      self.isNavigating = false
      state.setActive(nil)
      return true
    end
  elseif event.type == "mouseMotion" and self.isNavigating then
    local dx = (event.gx or event.x or 0) - self.dragStartX
    local dy = (event.gy or event.y or 0) - self.dragStartY

    -- Horizontal drag scrubs time (dragging right pulls forward towards NOW, dragging left pulls into history)
    local secPerPx = self.timeSpanSec / (self.props.w or 800)
    local newOffset = self.dragStartTimeOffset + (dx * secPerPx)
    self.timeOffsetSec = clamp(newOffset, 0.0, self.totalBufferSec - self.timeSpanSec)

    -- Vertical drag pans frequency
    local fSpan = self.freqMax - self.freqMin
    local hzPerPx = fSpan / (self.props.h or 450)
    local newCenter = self.dragStartCenterFreq + (dy * hzPerPx)
    self.freqMin = newCenter - fSpan / 2
    self.freqMax = newCenter + fSpan / 2
    return true
  elseif event.type == "mouseWheel" then
    -- Mouse wheel zooms frequency (or time if Shift/Ctrl)
    local factor = (event.delta > 0) and 0.85 or 1.15
    local center = (self.freqMin + self.freqMax) / 2
    local newSpan = (self.freqMax - self.freqMin) * factor
    if newSpan >= 10e3 and newSpan <= 2e6 then
      self.freqMin = center - newSpan / 2
      self.freqMax = center + newSpan / 2
    end
    return true
  end

  return false
end

return RiverView
