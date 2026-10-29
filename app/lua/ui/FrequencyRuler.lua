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
  self.contextMenuOpen = false
  self.bandMenuOpen = false
  self.tuneInputText = ""
  self.tuneInputActive = false
  self.bandList = { "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m" }
  self.zoomPresets = {
    { label = "300k", span = 300e3 },
    { label = "100k", span = 100e3 },
    { label = "50k", span = 50e3 },
    { label = "25k", span = 25e3 },
    { label = "10k", span = 10e3 }
  }
end

function FrequencyRuler:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 60 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 300 end
end

function FrequencyRuler:parseFrequency(str)
  if not str or #str == 0 then return nil end
  local clean = str:gsub("[^%d%.]", "")
  local val = tonumber(clean)
  if not val or val <= 0 then return nil end
  if val < 30.0 then
    -- Assume MHz (e.g. 14.074 -> 14.074e6)
    return val * 1e6
  elseif val < 30000.0 then
    -- Assume kHz (e.g. 14074 -> 14.074e6, 7030 -> 7.030e6)
    return val * 1e3
  else
    -- Assume Hz (e.g. 14074000)
    return val
  end
end

function FrequencyRuler:commitDirectTune()
  local f = self:parseFrequency(self.tuneInputText)
  if f then
    if self.onManualTune then
      self:onManualTune(f)
    end
    self.tuneInputText = ""
    self.contextMenuOpen = false
    self.bandMenuOpen = false
    self.tuneInputActive = false
  end
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

  local rulerH = self.props.rulerH or 22
  local canvasH = math.max(10, h - rulerH)

  -- 1. Band-Plan Mode Shading
  if _G.bands and _G.bands.getSegmentsForSpan then
    local segments = _G.bands.getSegmentsForSpan(fMin, fMax)
    for _, seg in ipairs(segments or {}) do
      local s0 = math.max(fMin, seg.startFreq)
      local s1 = math.min(fMax, seg.endFreq)
      if s1 > s0 then
        local yBottom = rulerH + canvasH - ((s0 - fMin) / span * canvasH)
        local yTop = rulerH + canvasH - ((s1 - fMin) / span * canvasH)
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
  local isMenuOpen = self.contextMenuOpen or self.bandMenuOpen
  local pillW = w - 8
  local pillH = 18
  local pillBg = isMenuOpen and { 0.2, 0.35, 0.55, 0.95 } or { 0.12, 0.18, 0.28, 0.85 }
  System.drawRoundedRect(4, 2, pillW, pillH, 3, pillBg)
  System.drawRectLines(4, 2, pillW, pillH, 1.0, { 0.3, 0.45, 0.65, 0.8 })
  local pillText = string.format("%s v", curBand)
  local ptw = System.measureText(pillText, 11) or (#pillText * 7)
  System.drawText(pillText, 4 + (pillW - ptw) / 2, 5, 11, { 0.9, 0.95, 1.0, 0.95 })

  -- 3. Ticks and Labels
  local idealTickCount = math.max(4, math.floor(canvasH / 36))
  local rawStep = span / idealTickCount
  local step = 50e3
  if rawStep <= 1e3 then step = 1e3
  elseif rawStep <= 2e3 then step = 2e3
  elseif rawStep <= 5e3 then step = 5e3
  elseif rawStep <= 10e3 then step = 10e3
  elseif rawStep <= 25e3 then step = 25e3
  elseif rawStep <= 50e3 then step = 50e3
  elseif rawStep <= 100e3 then step = 100e3
  else step = 200e3 end

  local firstTick = math.ceil(fMin / step) * step
  local cur = firstTick
  while cur <= fMax do
    local norm = (cur - fMin) / span
    local y = rulerH + canvasH - (norm * canvasH)

    if y >= rulerH then
      -- Tick mark
      System.drawLine(0, y, 6, y, 1.5, { 0.6, 0.65, 0.75, 0.9 })

      -- Label: e.g. "14.150"
      local mhz = cur / 1.0e6
      local label = string.format("%.3f", mhz)
      System.drawText(label, 9, y - 6, 12, { 0.8, 0.85, 0.9, 0.9 })
    end

    cur = cur + step
  end

  -- 4. Frequency Ruler Context Menu Popover (when open)
  if isMenuOpen then
    local menuW = 162
    local menuH = 260
    local menuX = -menuW - 4
    local menuY = 4

    -- Outer card background
    System.drawRoundedRect(menuX, menuY, menuW, menuH, 5, { 0.07, 0.1, 0.18, 0.98 })
    System.drawRectLines(menuX, menuY, menuW, menuH, 1.2, { 0.28, 0.42, 0.65, 0.95 })

    -- Title Header
    System.drawText("Frequency Ruler Menu", menuX + 8, menuY + 6, 11, { 0.75, 0.88, 1.0, 0.95 })
    System.drawLine(menuX + 6, menuY + 20, menuX + menuW - 6, menuY + 20, 1.0, { 0.2, 0.3, 0.45, 0.7 })

    -- Section 1: Manual Frequency Entry
    System.drawText("Tune Frequency:", menuX + 8, menuY + 26, 10, { 0.7, 0.75, 0.85, 0.9 })
    local inputW = 104
    local inputH = 20
    local inputX = menuX + 8
    local inputY = menuY + 40
    local inputBg = self.tuneInputActive and { 0.12, 0.18, 0.32, 0.95 } or { 0.09, 0.13, 0.22, 0.9 }
    local inputBorder = self.tuneInputActive and { 0.3, 0.7, 1.0, 0.95 } or { 0.25, 0.35, 0.5, 0.8 }
    System.drawRect(inputX, inputY, inputW, inputH, inputBg)
    System.drawRectLines(inputX, inputY, inputW, inputH, 1.0, inputBorder)

    local displayText = self.tuneInputText
    if #displayText == 0 then
      if self.tuneInputActive then
        displayText = "_"
      else
        displayText = "e.g. 14.074"
      end
    elseif self.tuneInputActive then
      displayText = displayText .. "_"
    end
    local textCol = (#self.tuneInputText > 0 or self.tuneInputActive) and { 1.0, 1.0, 1.0, 0.95 } or { 0.5, 0.55, 0.65, 0.7 }
    System.drawText(displayText, inputX + 4, inputY + 4, 11, textCol)

    -- [Tune] Button
    local goW = 38
    local goH = 20
    local goX = menuX + 116
    local goY = inputY
    System.drawRect(goX, goY, goW, goH, { 0.15, 0.45, 0.75, 0.9 })
    System.drawRectLines(goX, goY, goW, goH, 1.0, { 0.3, 0.65, 0.95, 0.9 })
    System.drawText("Tune", goX + 6, goY + 4, 10, { 1.0, 1.0, 1.0, 0.95 })

    -- Section 2: Band Selection Grid (3 columns)
    System.drawLine(menuX + 6, menuY + 66, menuX + menuW - 6, menuY + 66, 1.0, { 0.2, 0.3, 0.45, 0.7 })
    System.drawText("Select Band:", menuX + 8, menuY + 72, 10, { 0.7, 0.75, 0.85, 0.9 })

    local bBtnW = 46
    local bBtnH = 18
    local bStartX = menuX + 8
    local bStartY = menuY + 86
    for i, bName in ipairs(self.bandList) do
      local col = (i - 1) % 3
      local row = math.floor((i - 1) / 3)
      local bx = bStartX + col * (bBtnW + 3)
      local by = bStartY + row * (bBtnH + 3)

      local isCur = (bName == curBand)
      local bBg = isCur and { 0.22, 0.5, 0.85, 0.9 } or { 0.12, 0.16, 0.26, 0.85 }
      local bBorder = isCur and { 0.45, 0.75, 1.0, 0.95 } or { 0.22, 0.3, 0.45, 0.7 }
      System.drawRect(bx, by, bBtnW, bBtnH, bBg)
      System.drawRectLines(bx, by, bBtnW, bBtnH, 1.0, bBorder)
      local btw = System.measureText(bName, 10) or (#bName * 6)
      System.drawText(bName, bx + (bBtnW - btw) / 2, by + 3, 10, { 0.95, 0.95, 1.0, 0.95 })
    end

    -- Section 3: Zoom Presets
    local zoomY = bStartY + 4 * (bBtnH + 3) + 4
    System.drawLine(menuX + 6, zoomY, menuX + menuW - 6, zoomY, 1.0, { 0.2, 0.3, 0.45, 0.7 })
    System.drawText("Zoom Passband:", menuX + 8, zoomY + 6, 10, { 0.7, 0.75, 0.85, 0.9 })

    local zBtnW = 27
    local zBtnH = 17
    local zStartX = menuX + 8
    local zStartY = zoomY + 20
    for i, zp in ipairs(self.zoomPresets) do
      local zx = zStartX + (i - 1) * (zBtnW + 2)
      local isCurZoom = (math.abs(span - zp.span) < 5000)
      local zBg = isCurZoom and { 0.2, 0.45, 0.75, 0.9 } or { 0.12, 0.16, 0.26, 0.85 }
      local zBorder = isCurZoom and { 0.4, 0.7, 1.0, 0.9 } or { 0.22, 0.3, 0.45, 0.7 }
      System.drawRect(zx, zStartY, zBtnW, zBtnH, zBg)
      System.drawRectLines(zx, zStartY, zBtnW, zBtnH, 1.0, zBorder)
      local ztw = System.measureText(zp.label, 9) or (#zp.label * 5)
      System.drawText(zp.label, zx + (zBtnW - ztw) / 2, zStartY + 3, 9, { 0.9, 0.95, 1.0, 0.95 })
    end

    -- Section 4: Quick Action [Center 300k on VFO]
    local actionY = zStartY + zBtnH + 7
    local actW = menuW - 16
    local actH = 20
    local actX = menuX + 8
    System.drawRect(actX, actionY, actW, actH, { 0.15, 0.25, 0.4, 0.85 })
    System.drawRectLines(actX, actionY, actW, actH, 1.0, { 0.3, 0.5, 0.75, 0.8 })
    local actText = "Center 300k on VFO"
    local atw = System.measureText(actText, 10) or (#actText * 6)
    System.drawText(actText, actX + (actW - atw) / 2, actionY + 4, 10, { 0.85, 0.95, 1.0, 0.95 })
  end
end

function FrequencyRuler:onEvent(event)
  local isMenuOpen = self.contextMenuOpen or self.bandMenuOpen

  if event.type == "textInput" and isMenuOpen and self.tuneInputActive then
    if event.text and event.text:match("[%d%.]") then
      self.tuneInputText = self.tuneInputText .. event.text
      return true
    end
  elseif event.type == "key" then
    if isMenuOpen and self.tuneInputActive then
      if event.key == "BACKSPACE" or event.key == "259" then
        if event.isDown and #self.tuneInputText > 0 then
          self.tuneInputText = self.tuneInputText:sub(1, -2)
        end
        return true
      elseif event.key == "ENTER" and event.isDown then
        self:commitDirectTune()
        return true
      elseif event.key == "ESCAPE" and event.isDown then
        self.contextMenuOpen = false
        self.bandMenuOpen = false
        self.tuneInputActive = false
        return true
      end
    end
  elseif event.type == "mouseButton" then
    if event.button == "RIGHT" and event.isDown then
      self.contextMenuOpen = not self.contextMenuOpen
      self.bandMenuOpen = self.contextMenuOpen
      self.tuneInputActive = false
      return true
    elseif event.button == "LEFT" then
      if event.isDown then
        local lx = event.x or 0
        local ly = event.y or 0

        -- 1. Check clicks inside the Context Menu Popover
        if isMenuOpen then
          local menuW = 162
          local menuH = 260
          local menuX = -menuW - 4
          local menuY = 4

          if lx >= menuX and lx <= menuX + menuW and ly >= menuY and ly <= menuY + menuH then
            -- Click inside Tune Input box
            local inputX = menuX + 8
            local inputY = menuY + 40
            local inputW = 104
            local inputH = 20
            if lx >= inputX and lx <= inputX + inputW and ly >= inputY and ly <= inputY + inputH then
              self.tuneInputActive = true
              return true
            end

            -- Click inside [Tune] button
            local goX = menuX + 116
            local goY = inputY
            local goW = 38
            local goH = 20
            if lx >= goX and lx <= goX + goW and ly >= goY and ly <= goY + goH then
              self:commitDirectTune()
              return true
            end

            -- Click inside Band Selection Grid
            local bBtnW = 46
            local bBtnH = 18
            local bStartX = menuX + 8
            local bStartY = menuY + 86
            for i, bName in ipairs(self.bandList) do
              local col = (i - 1) % 3
              local row = math.floor((i - 1) / 3)
              local bx = bStartX + col * (bBtnW + 3)
              local by = bStartY + row * (bBtnH + 3)
              if lx >= bx and lx <= bx + bBtnW and ly >= by and ly <= by + bBtnH then
                Model.rx.selectedBand:set(bName)
                self.contextMenuOpen = false
                self.bandMenuOpen = false
                self.tuneInputActive = false
                if self.onSelectBand then self:onSelectBand(bName) end
                return true
              end
            end

            -- Click inside Zoom Presets
            local zoomY = bStartY + 4 * (bBtnH + 3) + 4
            local zBtnW = 27
            local zBtnH = 17
            local zStartX = menuX + 8
            local zStartY = zoomY + 20
            for i, zp in ipairs(self.zoomPresets) do
              local zx = zStartX + (i - 1) * (zBtnW + 2)
              if lx >= zx and lx <= zx + zBtnW and ly >= zStartY and ly <= zStartY + zBtnH then
                self.contextMenuOpen = false
                self.bandMenuOpen = false
                self.tuneInputActive = false
                if self.onZoomPreset then self:onZoomPreset(zp.span) end
                return true
              end
            end

            -- Click inside [Center 300k on VFO]
            local actionY = zStartY + zBtnH + 7
            local actW = menuW - 16
            local actH = 20
            local actX = menuX + 8
            if lx >= actX and lx <= actX + actW and ly >= actionY and ly <= actionY + actH then
              self.contextMenuOpen = false
              self.bandMenuOpen = false
              self.tuneInputActive = false
              if self.onCenterVFO then self:onCenterVFO() end
              return true
            end

            -- Clicked on empty space inside menu: don't dismiss
            self.tuneInputActive = false
            return true
          else
            -- Clicked outside menu: close context menu
            self.contextMenuOpen = false
            self.bandMenuOpen = false
            self.tuneInputActive = false
            return true
          end
        end

        -- 2. Clicking top pill header toggles Context Menu
        if ly <= 24 then
          self.contextMenuOpen = not self.contextMenuOpen
          self.bandMenuOpen = self.contextMenuOpen
          self.tuneInputActive = false
          return true
        end

        -- 3. Dragging on the ruler
        self.isDragging = true
        self.dragStartY = event.gy or event.y or 0
        self.dragStartCenter = (self.props.freqMin + self.props.freqMax) / 2
        state.setActive(self.id)
        return true
      else
        self.isDragging = false
        state.setActive(nil)
        if self.onFrequencyChange then
          self:onFrequencyChange(self.props.freqMin, self.props.freqMax, true)
        end
        return true
      end
    end
  elseif event.type == "mouseMotion" and self.isDragging then
    local dy = (event.gy or event.y or 0) - self.dragStartY
    local span = self.props.freqMax - self.props.freqMin
    local canvasH = math.max(10, (self.props.h or 300) - (self.props.rulerH or 22))
    local hzPerPx = span / canvasH
    local newCenter = self.dragStartCenter + (dy * hzPerPx)
    local fMin = newCenter - span / 2
    local fMax = newCenter + span / 2

    -- Pin edges strictly within the receiver's active 300 kHz passband
    local hwCenter = (Model.spectrumCenterFreq and Model.spectrumCenterFreq:peek()) or 14.2e6
    local bandMin = hwCenter - 150e3
    local bandMax = hwCenter + 150e3

    if fMin < bandMin then
      fMin = bandMin
      fMax = bandMin + span
    end
    if fMax > bandMax then
      fMax = bandMax
      fMin = bandMax - span
    end

    self.props.freqMin = fMin
    self.props.freqMax = fMax
    if self.onFrequencyChange then
      self:onFrequencyChange(self.props.freqMin, self.props.freqMax, false)
    end
    return true
  elseif event.type == "mouseWheel" then
    local river = self.parent and self.parent:findByName("id-river-view")
    if river and river.handleWheelZoom then
      return river:handleWheelZoom(event)
    end
    local factor = (event.delta > 0) and 0.85 or 1.15
    local center = (self.props.freqMin + self.props.freqMax) / 2
    local curSpan = self.props.freqMax - self.props.freqMin
    local newSpan = math.max(10e3, math.min(300e3, curSpan * factor))

    local hwCenter = (Model.spectrumCenterFreq and Model.spectrumCenterFreq:peek()) or 14.2e6
    local bandMin = hwCenter - 150e3
    local bandMax = hwCenter + 150e3

    local fMin = center - newSpan / 2
    local fMax = center + newSpan / 2
    if fMin < bandMin then
      fMin = bandMin
      fMax = bandMin + newSpan
    end
    if fMax > bandMax then
      fMax = bandMax
      fMin = bandMax - newSpan
    end
    if fMin < bandMin then fMin = bandMin end

    self.props.freqMin = fMin
    self.props.freqMax = fMax
    if self.onFrequencyChange then
      self:onFrequencyChange(self.props.freqMin, self.props.freqMax, true)
    end
    return true
  end
  return false
end

return FrequencyRuler
