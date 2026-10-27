--[[
  FrequencyRuler.lua - Vertical frequency ruler for The River

  Renders a vertical frequency scale (bottom = freqMin, top = freqMax)
  with band-plan boundary shading (CW, DATA, USB/LSB) behind tick marks.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")
local Model = require("Model")

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
  self.bandMenuOpen = false
  self.bandList = { "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m" }
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

  -- 2. Band Pill Header at the top of the ruler
  local curBand = (Model.rx.selectedBand and Model.rx.selectedBand:get()) or "20m"
  local pillW = w - 8
  local pillH = 20
  local pillBg = self.bandMenuOpen and { 0.2, 0.35, 0.55, 0.95 } or { 0.12, 0.18, 0.28, 0.85 }
  System.drawRoundedRect(4, 2, pillW, pillH, 3, pillBg)
  System.drawRectLines(4, 2, pillW, pillH, 1.0, { 0.3, 0.45, 0.65, 0.8 })
  local pillText = string.format("%s v", curBand)
  local ptw = System.measureText(pillText, 11) or (#pillText * 7)
  System.drawText(pillText, 4 + (pillW - ptw) / 2, 6, 11, { 0.9, 0.95, 1.0, 0.95 })

  -- 3. Ticks and Labels
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

    if y >= 24 then
      -- Tick mark
      System.drawLine(0, y, 6, y, 1.5, { 0.6, 0.65, 0.75, 0.9 })

      -- Label: e.g. "14.150"
      local mhz = cur / 1.0e6
      local label = string.format("%.3f", mhz)
      System.drawText(label, 9, y - 6, 12, { 0.8, 0.85, 0.9, 0.9 })
    end

    cur = cur + step
  end

  -- 4. Floating Band Selection Popover Menu (when open)
  if self.bandMenuOpen then
    local menuW = 76
    local menuX = -menuW - 2
    local itemH = 22
    local menuH = #self.bandList * itemH + 6
    System.drawRoundedRect(menuX, 4, menuW, menuH, 4, { 0.08, 0.12, 0.2, 0.98 })
    System.drawRectLines(menuX, 4, menuW, menuH, 1.0, { 0.25, 0.35, 0.5, 0.9 })

    for i, bName in ipairs(self.bandList) do
      local iy = 7 + (i - 1) * itemH
      if bName == curBand then
        System.drawRect(menuX + 3, iy, menuW - 6, itemH - 2, { 0.2, 0.4, 0.7, 0.85 })
      end
      local bw = System.measureText(bName, 11) or (#bName * 7)
      System.drawText(bName, menuX + (menuW - bw) / 2, iy + 4, 11, { 1.0, 1.0, 1.0, 0.95 })
    end
  end
end

function FrequencyRuler:onEvent(event)
  if event.type == "mouseButton" then
    if event.button == "RIGHT" and event.isDown then
      self.bandMenuOpen = not self.bandMenuOpen
      return true
    elseif event.button == "LEFT" then
      if event.isDown then
        local lx = event.x or 0
        local ly = event.y or 0

        -- Check if Band Menu is open and clicked
        if self.bandMenuOpen then
          local menuW = 76
          local menuX = -menuW - 2
          local itemH = 22
          local menuH = #self.bandList * itemH + 6
          if lx >= menuX and lx <= menuX + menuW and ly >= 4 and ly <= 4 + menuH then
            local idx = math.floor((ly - 7) / itemH) + 1
            if idx >= 1 and idx <= #self.bandList then
              local chosen = self.bandList[idx]
              Model.rx.selectedBand:set(chosen)
              self.bandMenuOpen = false
              if self.onSelectBand then self:onSelectBand(chosen) end
              return true
            end
          end
          self.bandMenuOpen = false
          return true
        end

        -- Clicking top pill toggles Band Menu
        if ly <= 24 then
          self.bandMenuOpen = not self.bandMenuOpen
          return true
        end

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
    local river = self.parent and self.parent:findByName("id-river-view")
    if river and river.handleWheelZoom then
      return river:handleWheelZoom(event)
    end
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
