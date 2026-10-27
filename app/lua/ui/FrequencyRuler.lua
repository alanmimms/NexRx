--[[
  FrequencyRuler.lua - Vertical frequency ruler for The River

  Renders a vertical frequency scale (bottom = freqMin, top = freqMax)
  with band-plan boundary shading (CW, DATA, USB/LSB) behind tick marks.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")

local FrequencyRuler = Widget.mkType("FrequencyRuler", Widget)

setbox.rule {
  id = "frequency-ruler-defaults",
  tags = { "widget.FrequencyRuler" },
  priority = -1000,
  apply = {
    prefW = 60,
    background = "#0c101c",
    border = "#1e293b",
    tickColor = "#94a3b8",
    textColor = "#cbd5e1",
    cwColor = { 0.15, 0.45, 0.85, 0.18 },
    dataColor = { 0.85, 0.65, 0.15, 0.18 },
    voiceColor = { 0.15, 0.75, 0.35, 0.18 },
  }
}

function FrequencyRuler:init(def)
  Widget.init(self, def)
  self.isDragging = false
  self.dragStartY = 0
  self.dragStartCenter = 0
end

function FrequencyRuler:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 60 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 300 end
end

function FrequencyRuler:drawSelf()
  local w = self.props.w or 0
  local h = self.props.h or 0
  if w <= 0 or h <= 0 then return end

  local fMin = self.props.freqMin or 14.0e6
  local fMax = self.props.freqMax or 14.35e6
  if fMax <= fMin then fMax = fMin + 1000 end
  local span = fMax - fMin

  local bgR, bgG, bgB = state.hexToRgb(self.props.background or "#0c101c")
  System.drawRect(0, 0, w, h, { bgR, bgG, bgB, 0.95 })
  System.drawRectLines(0, 0, w, h, 1, { 0.15, 0.2, 0.3, 0.8 })

  -- 1. Band-Plan Mode Shading
  if _G.bands and _G.bands.getSegmentsForSpan then
    local segments = _G.bands.getSegmentsForSpan(fMin, fMax)
    for _, seg in ipairs(segments or {}) do
      local s0 = math.max(fMin, seg.startFreq)
      local s1 = math.min(fMax, seg.endFreq)
      if s1 > s0 then
        local yBottom = h - ((s0 - fMin) / span * h)
        local yTop = h - ((s1 - fMin) / span * h)
        local segH = math.max(1, yBottom - yTop)

        local color = self.props.voiceColor
        local modeUpper = (seg.mode or ""):upper()
        if modeUpper:find("CW") then
          color = self.props.cwColor
        elseif modeUpper:find("DATA") or modeUpper:find("RTTY") or modeUpper:find("FT8") then
          color = self.props.dataColor
        end
        System.drawRect(0, yTop, w - 8, segH, color)
      end
    end
  end

  -- 2. Ticks and Labels
  -- Choose nice step (e.g. 10 kHz, 25 kHz, 50 kHz, 100 kHz)
  local idealTickCount = math.max(4, math.floor(h / 36))
  local rawStep = span / idealTickCount
  local step = 50e3
  if rawStep <= 10e3 then step = 10e3
  elseif rawStep <= 25e3 then step = 25e3
  elseif rawStep <= 50e3 then step = 50e3
  elseif rawStep <= 100e3 then step = 100e3
  else step = 200e3 end

  local firstTick = math.ceil(fMin / step) * step
  local cur = firstTick
  while cur <= fMax do
    local norm = (cur - fMin) / span
    local y = h - (norm * h)

    -- Tick mark
    System.drawLine(0, y, 6, y, 1.5, { 0.6, 0.65, 0.75, 0.9 })

    -- Label: e.g. "14.150"
    local mhz = cur / 1.0e6
    local label = string.format("%.3f", mhz)
    System.drawText(label, 9, y - 6, 12, { 0.8, 0.85, 0.9, 0.9 })

    cur = cur + step
  end
end

function FrequencyRuler:onEvent(event)
  if event.type == "mouseButton" and event.button == "LEFT" then
    if event.isDown then
      self.isDragging = true
      self.dragStartY = event.gy or event.y or 0
      self.dragStartCenter = (self.props.freqMin + self.props.freqMax) / 2
      state.setActive(self.id)
      return true
    else
      self.isDragging = false
      state.setActive(nil)
      return true
    end
  elseif event.type == "mouseMotion" and self.isDragging then
    local dy = (event.gy or event.y or 0) - self.dragStartY
    local span = self.props.freqMax - self.props.freqMin
    local hzPerPx = span / (self.props.h or 300)
    -- Dragging down pulls lower frequencies down
    local newCenter = self.dragStartCenter + (dy * hzPerPx)
    self.props.freqMin = newCenter - span / 2
    self.props.freqMax = newCenter + span / 2
    if self.onFrequencyChange then
      self:onFrequencyChange(self.props.freqMin, self.props.freqMax)
    end
    return true
  elseif event.type == "mouseWheel" then
    local factor = (event.delta > 0) and 0.85 or 1.15
    local center = (self.props.freqMin + self.props.freqMax) / 2
    local newSpan = (self.props.freqMax - self.props.freqMin) * factor
    if newSpan >= 10e3 and newSpan <= 2e6 then
      self.props.freqMin = center - newSpan / 2
      self.props.freqMax = center + newSpan / 2
      if self.onFrequencyChange then
        self:onFrequencyChange(self.props.freqMin, self.props.freqMax)
      end
    end
    return true
  end
  return false
end

return FrequencyRuler
