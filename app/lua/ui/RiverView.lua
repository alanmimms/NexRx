--[[
  RiverView.lua - The River primary viewport and spectral workspace

  Implements the split-river canvas, top time ruler, CAD discontinuity
  break line, live-pinned region, inline decoded tracks, bubble loupe,
  and 2D time-frequency navigation.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")
local Model = require("Model")
local SignalBox = require("ui.SignalBox")
local RiverBehaviors = require("ui.RiverBehaviors")
local DecoderTrack = require("ui.DecoderTrack")

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
    gridColor = { 0.22, 0.32, 0.46, 0.18 },
    pinnedSliverW = 60,
    cadBreakW = 0,
    historyDragThresholdPx = 40,
    rulerH = 22,
  }
}

function RiverView:init(def)
  def = def or {}
  Widget.Container.init(self, def)

  self.timeSpanSec = def.timeSpanSec or def.timeSpan or 30.0
  self.timeOffsetSec = def.timeOffsetSec or def.timeOffset or 0.0
  self.totalBufferSec = def.totalBufferSec or def.totalBuffer or 60.0

  self.freqMin = def.freqMin or 14.0e6
  self.freqMax = def.freqMax or 14.35e6
  self.customCenterFreq = (def.centerFreq ~= nil)
  self.customBandwidth = (def.bandwidth ~= nil)
  self.hwCenterFreq = def.centerFreq or (Model.spectrumCenterFreq and Model.spectrumCenterFreq:peek()) or 14.2e6
  self.hwBandwidth = def.bandwidth or _G.sampleRate or 384e3

  self.isNavigating = false
  self.dragAxis = nil
  self.historyDragThresholdPx = def.historyDragThresholdPx or 40
  self.dragStartX = 0
  self.dragStartY = 0
  self.dragStartTimeOffset = 0
  self.dragStartCenterFreq = 0

  self.hoverActive = false
  self.hoverX = 0
  self.hoverY = 0

  self.signalBoxes = {}
  if def.signalBoxes then
    for _, boxDef in ipairs(def.signalBoxes) do
      self:addSignalBox(boxDef)
    end
  end

  self.decoderTracks = {}
  if def.decoderTracks then
    for _, tDef in ipairs(def.decoderTracks) do
      self:addDecoderTrack(tDef)
    end
  end
end

function RiverView:addSignalBox(boxDef)
  local box = SignalBox(boxDef)
  table.insert(self.signalBoxes, box)
  self:add(box)
  return box
end

function RiverView:addDecoderTrack(trackDef)
  local track = (trackDef.renderInline) and trackDef or DecoderTrack(trackDef)
  table.insert(self.decoderTracks, track)
  return track
end

function RiverView:getFreqAtY(yPx)
  local rulerH = self.props.rulerH or 22
  local canvasH = math.max(10, (self.props.h or 450) - rulerH)
  local relY = clamp(yPx - rulerH, 0, canvasH)
  local normY = 1.0 - (relY / canvasH)
  local fSpan = math.max(1000, self.freqMax - self.freqMin)
  return self.freqMin + (normY * fSpan)
end

local loadedStimuli = nil

local function getStimuliFromTwin()
  if loadedStimuli then return loadedStimuli end
  local list = {}
  local fakeStimulus = {
    addNoise = function(name, def) end,
    addMorse = function(name, def)
      local isW1AW = (name == "cq-WA1AW") or (def.text and def.text:match("W1AW"))
      table.insert(list, {
        freq = def.freq,
        mode = "CW",
        bw = 500,
        label = isW1AW and "W1AW CQ" or "CW Beacon",
        caller = isW1AW and "W1AW" or "WWV",
        tol = 6e3,
      })
    end,
    addSsb = function(name, def)
      local mode = (def.mode or "usb"):upper()
      local caller = nil
      local label = name
      if name == "voice-id" or (def.audioFile and def.audioFile:match("WB7NAB")) then
        caller = "WB7NAB"
        label = "CQ WB7NAB"
      elseif name == "ssb-2tone" then
        label = "2-Tone"
      elseif name == "ssb-1tone" then
        label = "SSB Tone"
      end
      table.insert(list, {
        freq = def.freq,
        mode = mode,
        bw = 2800,
        label = label,
        caller = caller,
        tol = 6e3,
      })
    end,
    addAm = function(name, def)
      local label = name
      local caller = nil
      local isWWV = (name == "wwv15") or (def.freq and math.abs(def.freq - 15e6) < 1e5) or (def.audioFile and def.audioFile:match("wwv"))
      if isWWV then
        label = "WWV 15"
        caller = "WWV"
      elseif name == "am-2tone" then
        label = "AM Beacon"
      end
      table.insert(list, {
        freq = def.freq,
        mode = "AM",
        bw = isWWV and 10000 or 6000,
        label = label,
        caller = caller,
        tol = isWWV and 50e3 or 6e3,
      })
    end,
    remove = function() end,
    clear = function() end,
    list = function() return {} end,
    count = function() return 0 end,
  }

  local candidatePaths = {
    "twin/config/stimuli/default.lua",
    "../twin/config/stimuli/default.lua",
    "/home/alan/ham/NexRx/twin/config/stimuli/default.lua"
  }
  for _, path in ipairs(candidatePaths) do
    local f = io.open(path, "r")
    if f then
      local content = f:read("*a")
      f:close()
      local env = {
        stimulus = fakeStimulus,
        print = function() end,
        string = string,
        pairs = pairs,
      }
      local fn = load(content, path, "t", env)
      if fn then
        pcall(fn)
        if #list > 0 then
          loadedStimuli = list
          return loadedStimuli
        end
      end
    end
  end

  -- Default fallback stimuli table
  loadedStimuli = {
    { freq = 15.0000e6, mode = "AM",  bw = 10000, label = "WWV 15", caller = "WWV", tol = 50e3 },
    { freq = 14.2500e6, mode = "AM",  bw = 6000,  label = "AM Beacon", tol = 6e3 },
    { freq = 14.0750e6, mode = "CW",  bw = 500,   label = "CW Beacon", caller = "WWV", tol = 6e3 },
    { freq = 14.0250e6, mode = "CW",  bw = 500,   label = "W1AW CQ", caller = "W1AW", tol = 6e3 },
    { freq = 14.1200e6, mode = "USB", bw = 2800,  label = "2-Tone", tol = 6e3 },
    { freq = 14.1500e6, mode = "USB", bw = 2800,  label = "SSB Tone", tol = 6e3 },
    { freq = 14.2000e6, mode = "USB", bw = 2800,  label = "CQ WB7NAB", caller = "WB7NAB", tol = 12e3 },
  }
  return loadedStimuli
end

function RiverView:getAutoSignalDef(freq)
  local stimuli = getStimuliFromTwin()
  for _, stim in ipairs(stimuli) do
    local tol = stim.tol or 10e3
    if math.abs(freq - stim.freq) <= tol then
      return stim.mode, stim.bw, stim.label, stim.caller
    end
  end

  local defaultMode = (freq >= 8.0e6) and "USB" or "LSB"

  if _G.bands and _G.bands.getSegmentsForSpan then
    local segs = _G.bands.getSegmentsForSpan(freq - 100, freq + 100)
    if segs and #segs > 0 and segs[1].mode then
      local m = segs[1].mode
      if m == "SSB" or m == "PHONE" then
        m = defaultMode
      end
      local bw = (m == "CW") and 500 or ((m == "AM") and 6000 or 2800)
      return m, bw, m, nil
    end
  end

  return defaultMode, 2800, defaultMode, nil
end

function RiverView:getAutoModeForFreq(freq)
  local mode = self:getAutoSignalDef(freq)
  return mode
end

function RiverView:cycleSignalBoxFocus(reverse)
  if #self.signalBoxes <= 1 then return end
  local cur = Model.selectedSignalBoxIndex:get() or 1
  local nextIdx = reverse and (cur - 1) or (cur + 1)
  if nextIdx < 1 then nextIdx = #self.signalBoxes end
  if nextIdx > #self.signalBoxes then nextIdx = 1 end

  Model.selectedSignalBoxIndex:set(nextIdx)
  local box = self.signalBoxes[nextIdx]
  if box then
    box:setFocus()
    if box.carrierFreq then
      Model.set("rx.VFO.activeValue", box.carrierFreq)
    end
    if box.mode then
      Model.set("rx.selectedMode", box.mode)
    end
  end
  self:layout(self.props.x or 0, self.props.y or 0, self.props.w or 800, self.props.h or 450)
  if self.onSignalBoxesChanged then self:onSignalBoxesChanged(self.signalBoxes) end
end

function RiverView:spawnSignalBox(freq)
  local newIdx = #self.signalBoxes + 1
  local autoMode, autoBw, autoLabel, autoCaller = self:getAutoSignalDef(freq)
  local newBox = self:addSignalBox({
    id = "box-" .. tostring(newIdx),
    index = newIdx,
    mode = autoMode,
    carrierFreq = freq,
    bandwidth = autoBw or ((autoMode == "CW") and 500 or 2800),
    label = autoLabel or ("BOX " .. tostring(newIdx)),
    caller = autoCaller
  })
  Model.selectedSignalBoxIndex:set(newIdx)
  Model.set("rx.VFO.activeValue", freq)
  Model.set("rx.selectedMode", autoMode)
  Model.set("rx.bandpass.width", newBox.bandwidth)
  newBox:setFocus()
  self:layout(self.props.x or 0, self.props.y or 0, self.props.w or 800, self.props.h or 450)
  if self.onSignalBoxesChanged then self:onSignalBoxesChanged(self.signalBoxes) end
  return newBox
end

function RiverView:tuneActiveSignalBox(freq)
  local curIdx = Model.selectedSignalBoxIndex:get() or 1
  local box = self.signalBoxes[curIdx]
  if not box then
    return self:spawnSignalBox(freq)
  end

  box.carrierFreq = freq
  local autoMode, autoBw, autoLabel, autoCaller = self:getAutoSignalDef(freq)
  if autoMode and autoMode ~= box.mode then
    box:setMode(autoMode)
    if autoBw then box.bandwidth = autoBw end
    Model.set("rx.bandpass.width", box.bandwidth)
  end
  if not box.isLabelLocked then
    box.label = autoLabel or ("BOX " .. tostring(curIdx))
    box.callerCallsign = autoCaller
    box.respondentCallsign = nil
  end
  Model.set("rx.VFO.activeValue", freq)
  box:setFocus()
  self:layout(self.props.x or 0, self.props.y or 0, self.props.w or 800, self.props.h or 450)
  if self.onSignalBoxesChanged then self:onSignalBoxesChanged(self.signalBoxes) end
  return box
end

function RiverView:removeFocusedSignalBox()
  if #self.signalBoxes <= 1 then return false end
  local curIdx = Model.selectedSignalBoxIndex:get() or 1
  local box = self.signalBoxes[curIdx]
  if not box then return false end

  for i, kid in ipairs(self.kids) do
    if kid == box then
      table.remove(self.kids, i)
      break
    end
  end

  table.remove(self.signalBoxes, curIdx)
  for i, b in ipairs(self.signalBoxes) do
    b.boxIndex = i
    b.id = "box-" .. tostring(i)
  end

  local nextIdx = math.min(curIdx, #self.signalBoxes)
  Model.selectedSignalBoxIndex:set(nextIdx)
  local newFocused = self.signalBoxes[nextIdx]
  if newFocused then
    newFocused:setFocus()
    Model.set("rx.VFO.activeValue", newFocused.carrierFreq)
    Model.set("rx.selectedMode", newFocused.mode)
  end

  self:layout(self.props.x or 0, self.props.y or 0, self.props.w or 800, self.props.h or 450)
  if self.onSignalBoxesChanged then self:onSignalBoxesChanged(self.signalBoxes) end
  return true
end

function RiverView:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 800 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 450 end
  for _, kid in ipairs(self.kids or {}) do
    kid:calcMetrics()
  end
end

function RiverView:getNormalizedFreqRange()
  if not self.customCenterFreq and Model.spectrumCenterFreq then
    self.hwCenterFreq = Model.spectrumCenterFreq:peek()
  end
  if not self.customBandwidth and _G.sampleRate then
    self.hwBandwidth = _G.sampleRate
  end

  local hwCenter = self.hwCenterFreq
  local hwSpan = self.hwBandwidth

  local hwMin = hwCenter - hwSpan / 2.0
  local fMinNorm = (self.freqMin - hwMin) / hwSpan
  local fMaxNorm = (self.freqMax - hwMin) / hwSpan
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
  self.lastLaidOutSpan = fSpan
  self.lastLaidOutMin = self.freqMin
end

function RiverView:drawSelf()
  local w = self.props.w or 0
  local h = self.props.h or 0
  if w <= 0 or h <= 0 then return end

  local rulerH = self.props.rulerH or 22
  local canvasY = rulerH
  local canvasH = math.max(10, h - rulerH)
  local fSpan = math.max(1000, self.freqMax - self.freqMin)
  local hzPerPx = fSpan / canvasH

  -- Re-layout SignalBoxes if frequency zoom or bounds have changed
  if self.lastLaidOutSpan ~= fSpan or self.lastLaidOutMin ~= self.freqMin then
    self:layout(self.props.x or 0, self.props.y or 0, w, h)
  end

  local fMinNorm, fMaxNorm = self:getNormalizedFreqRange()
  local riverEngine = _G.river or _G.waterfall
  local ox, oy = state.getOffset()

  -- Fill river canvas background to ensure clean appearance outside hardware capture bandwidth
  local bgR, bgG, bgB = state.hexToRgb(self.props.background or "#050811")
  System.drawRect(0, canvasY, w, canvasH, { bgR, bgG, bgB, 1.0 })

  local pinnedW = self.props.pinnedSliverW or 60
  local isSplit = (self.timeOffsetSec > 0.01)

  -- 1. Main River Texture Pass
  local breakW = 0
  local historyW = isSplit and math.max(20, w - pinnedW) or w

  if riverEngine and riverEngine.renderRiver then
    if not isSplit then
      -- Continuous single River canvas flush to NOW line
      local visSpanNorm = clamp(self.timeSpanSec / self.totalBufferSec, 0.05, 1.0)
      riverEngine.renderRiver(ox, oy + canvasY, w, canvasH,
                              visSpanNorm, 0.0, fMinNorm, fMaxNorm)
    else
      -- Split River: Scrolled History Region + Real-time Live Pinned Region (1-2s sliver)
      -- Seamless texture underneath the zigzag line with zero background/gap
      local histSpanNorm = clamp(self.timeSpanSec / self.totalBufferSec, 0.05, 1.0)
      local histOffsetNorm = clamp(self.timeOffsetSec / self.totalBufferSec, 0.0, 1.0 - histSpanNorm)

      riverEngine.renderRiver(ox, oy + canvasY, historyW, canvasH,
                              histSpanNorm, histOffsetNorm, fMinNorm, fMaxNorm)

      local liveSpanNorm = clamp(2.0 / self.totalBufferSec, 0.02, 0.1)
      riverEngine.renderRiver(ox + historyW, oy + canvasY, pinnedW, canvasH,
                              liveSpanNorm, 0.0, fMinNorm, fMaxNorm)
    end
  end

  -- CAD Break Zigzag Line and Jump Scroll for Split River (drawn without any background)
  if isSplit then
    local bx = historyW
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
    local dw = System.measureText(dimText, 11) or (#dimText * 8)
    System.drawText(dimText, bx - dw / 2, 4, 11, dimColor)

    -- 50% Jump-Scroll when playback line reaches right boundary of history
    for _, box in ipairs(self.signalBoxes or {}) do
      if box.playbackActive and box.playbackX and box.playbackX >= historyW - 10 then
        local RiverBehaviors = require("ui.RiverBehaviors")
        self.timeOffsetSec = RiverBehaviors.jumpScroll.calculateJumpTarget(self.timeOffsetSec, self.timeSpanSec, 0.5)
        break
      end
    end
  end

  -- 2. Faint Grid Lines across River Canvas (Frequency & Time Ticks)
  local gridColor = self.props.gridColor or { 0.22, 0.32, 0.46, 0.18 }
  if gridColor and (gridColor[4] or 0) > 0 then
    -- A. Horizontal Frequency Grid Lines (matching FrequencyRuler steps)
    local idealTickCount = math.max(4, math.floor(canvasH / 36))
    local rawFStep = fSpan / idealTickCount
    local fStep = 50e3
    if rawFStep <= 1e3 then fStep = 1e3
    elseif rawFStep <= 2e3 then fStep = 2e3
    elseif rawFStep <= 5e3 then fStep = 5e3
    elseif rawFStep <= 10e3 then fStep = 10e3
    elseif rawFStep <= 25e3 then fStep = 25e3
    elseif rawFStep <= 50e3 then fStep = 50e3
    elseif rawFStep <= 100e3 then fStep = 100e3
    else fStep = 200e3 end

    local firstFreq = math.ceil(self.freqMin / fStep) * fStep
    local curF = firstFreq
    while curF <= self.freqMax do
      local normF = (curF - self.freqMin) / fSpan
      local gy = canvasY + canvasH - (normF * canvasH)
      if gy >= canvasY and gy <= canvasY + canvasH then
        System.drawLine(0, gy, w, gy, 1.0, gridColor)
      end
      curF = curF + fStep
    end

    -- B. Vertical Time Grid Lines (matching time ticks)
    local tSpan = self.timeSpanSec or 30.0
    local tStep = 5.0
    if tSpan <= 10.0 then tStep = 1.0
    elseif tSpan <= 20.0 then tStep = 2.0
    elseif tSpan <= 40.0 then tStep = 5.0
    elseif tSpan <= 80.0 then tStep = 10.0
    else tStep = 15.0 end

    if not isSplit then
      local curSec = tStep
      while curSec < tSpan do
        local normT = 1.0 - (curSec / tSpan)
        local gx = normT * w
        if gx > 0 and gx < w then
          System.drawLine(gx, canvasY, gx, canvasY + canvasH, 1.0, gridColor)
        end
        curSec = curSec + tStep
      end
    else
      local tEnd = self.timeOffsetSec
      local tStart = self.timeOffsetSec + self.timeSpanSec
      local firstT = math.ceil(tEnd / tStep) * tStep
      if firstT == tEnd then firstT = firstT + tStep end
      local curT = firstT
      while curT < tStart do
        local normT = 1.0 - ((curT - tEnd) / tSpan)
        local gx = normT * historyW
        if gx > 0 and gx < historyW then
          System.drawLine(gx, canvasY, gx, canvasY + canvasH, 1.0, gridColor)
        end
        curT = curT + tStep
      end
    end
  end

  -- 3. Inline Decoded Tracks & Bubble Zoom Loupe (Requires ALT modifier)
  local isAlt = self.isAltDown or (_G.isAltDown and _G.isAltDown()) or false
  local loupeActive = false
  if isAlt and self.hoverActive and self.hoverY and #self.decoderTracks > 0 then
    local nearby = {}
    for _, tr in ipairs(self.decoderTracks) do
      local ty = canvasY + canvasH - ((tr.freq - self.freqMin) / fSpan * canvasH)
      table.insert(nearby, { y = ty })
    end
    loupeActive = RiverBehaviors.bubbleLoupe.shouldActivateBubble(self.hoverX, self.hoverY, nearby, true)
  end
  self.loupeActive = loupeActive

  for _, tr in ipairs(self.decoderTracks or {}) do
    if tr.freq >= self.freqMin and tr.freq <= self.freqMax then
      local ty = canvasY + canvasH - ((tr.freq - self.freqMin) / fSpan * canvasH)
      tr:renderInline(ty, w, self.timeSpanSec, hzPerPx, loupeActive, self.hoverX, self.hoverY)
    end
  end

  if loupeActive and self.hoverX and self.hoverY then
    local rad = RiverBehaviors.bubbleLoupe.radius or 60
    System.drawCircleOutline(self.hoverX, self.hoverY, rad, 2.0, { 0.35, 0.85, 1.0, 0.85 })
    System.drawCircleOutline(self.hoverX, self.hoverY, rad + 1, 1.0, { 0.15, 0.45, 0.8, 0.35 })
    System.drawLine(self.hoverX - rad, self.hoverY, self.hoverX - rad + 8, self.hoverY, 1.0, { 0.35, 0.85, 1.0, 0.6 })
    System.drawLine(self.hoverX + rad - 8, self.hoverY, self.hoverX + rad, self.hoverY, 1.0, { 0.35, 0.85, 1.0, 0.6 })
    System.drawLine(self.hoverX, self.hoverY - rad, self.hoverX, self.hoverY - rad + 8, 1.0, { 0.35, 0.85, 1.0, 0.6 })
    System.drawLine(self.hoverX, self.hoverY + rad - 8, self.hoverX, self.hoverY + rad, 1.0, { 0.35, 0.85, 1.0, 0.6 })
  end

  -- 4. Top Time Ruler
  local rBgR, rBgG, rBgB = state.hexToRgb(self.props.rulerBg or "#0a0f1d")
  System.drawRect(0, 0, w, rulerH, { rBgR, rBgG, rBgB, 0.95 })
  System.drawLine(0, rulerH, w, rulerH, 1.0, { 0.2, 0.25, 0.35, 0.8 })

  local tSpan = self.timeSpanSec or 30.0
  local tStep = 5.0
  if tSpan <= 10.0 then tStep = 1.0
  elseif tSpan <= 20.0 then tStep = 2.0
  elseif tSpan <= 40.0 then tStep = 5.0
  elseif tSpan <= 80.0 then tStep = 10.0
  else tStep = 15.0 end

  if not isSplit then
    -- Live time markers: e.g. -25s, -20s, -15s, -10s, -5s, NOW
    local curSec = tStep
    while curSec < tSpan do
      local normT = 1.0 - (curSec / tSpan)
      local tx = normT * w
      if tx > 20 and tx < w - 35 then
        System.drawLine(tx, rulerH - 6, tx, rulerH, 1.0, { 0.45, 0.55, 0.70, 0.85 })
        local tText = string.format("-%ds", math.floor(curSec + 0.5))
        local tw = System.measureText(tText, 11) or (#tText * 7)
        System.drawText(tText, math.floor(tx - tw / 2), 4, 11, { 0.75, 0.82, 0.90, 0.9 })
      end
      curSec = curSec + tStep
    end
    System.drawText("NOW", w - 34, 4, 11, { 0.25, 0.95, 0.4, 0.95 })
  else
    -- Scrolled historical markers on history region
    local tEnd = self.timeOffsetSec
    local tStart = self.timeOffsetSec + self.timeSpanSec
    local firstT = math.ceil(tEnd / tStep) * tStep
    if firstT == tEnd then firstT = firstT + tStep end
    local curT = firstT
    while curT < tStart do
      local normT = 1.0 - ((curT - tEnd) / tSpan)
      local tx = normT * historyW
      if tx > 20 and tx < historyW - 20 then
        System.drawLine(tx, rulerH - 6, tx, rulerH, 1.0, { 0.45, 0.55, 0.70, 0.85 })
        local tText = string.format("-%ds", math.floor(curT + 0.5))
        local tw = System.measureText(tText, 11) or (#tText * 7)
        System.drawText(tText, math.floor(tx - tw / 2), 4, 11, { 0.75, 0.82, 0.90, 0.9 })
      end
      curT = curT + tStep
    end
    System.drawText("NOW", w - 34, 4, 11, { 0.25, 0.95, 0.4, 0.95 })
  end
end

function RiverView:onEvent(event)
  if event.type == "key" then
    if event.key == "ALT" or event.key == "LEFT_ALT" or event.key == "RIGHT_ALT" then
      self.isAltDown = event.isDown or false
      return true
    end
    -- TAB / Shift+TAB to cycle SignalBox focus
    if event.key == "TAB" and event.isDown then
      local isShift = (event.modifiers and (event.modifiers.shift or (event.modifiers[1] == "input.SHIFT"))) or (_G.isShiftDown and _G.isShiftDown())
      self:cycleSignalBoxFocus(isShift)
      return true
    end

    -- DELETE / BACKSPACE to remove focused secondary SignalBox
    if (event.key == "DELETE" or event.key == "BACKSPACE") and event.isDown then
      if self:removeFocusedSignalBox() then
        return true
      end
    end
  end

  if event.type == "mouseButton" and event.button == "LEFT" then
    local rx = event.x or 0
    local ry = event.y or 0
    local w = self.props.w or 800
    local pinnedW = self.props.pinnedSliverW or 60

    if event.isDown then
      -- If click is inside active bubble loupe, invert coordinates so click targets true unmagnified frequency
      if self.loupeActive and self.hoverX and self.hoverY then
        local rad = RiverBehaviors.bubbleLoupe.radius or 60
        local cDx = rx - self.hoverX
        local cDy = ry - self.hoverY
        if (cDx * cDx + cDy * cDy) <= (rad * rad) then
          rx, ry = RiverBehaviors.bubbleLoupe.transformInverse(rx, ry, self.hoverX, self.hoverY, 1.5, 2.5)
        end
      end

      -- Clicking in live pinned sliver snaps back to real time
      if self.timeOffsetSec > 0 and rx >= w - pinnedW then
        self.timeOffsetSec = 0.0
        return true
      end

      -- ALT+Click drops a new SignalBox
      local isAlt = false
      if event.modifiers then
        if event.modifiers.alt or event.modifiers.ALT then isAlt = true end
        if type(event.modifiers) == "table" then
          for _, m in ipairs(event.modifiers) do
            if m == "input.ALT" or m == "alt" or m == "ALT" then isAlt = true end
          end
        end
      end
      if _G.isAltDown and _G.isAltDown() then isAlt = true end
      if self.isAltDown then isAlt = true end
      if isAlt then
        local clickFreq = self:getFreqAtY(ry)
        self:spawnSignalBox(clickFreq)
        return true
      end

      self.isNavigating = true
      self.dragAxis = nil
      self.clickStartX = rx
      self.clickStartY = ry
      self.dragStartX = event.gx or event.x or 0
      self.dragStartY = event.gy or event.y or 0
      self.dragStartTimeOffset = self.timeOffsetSec
      self.dragStartCenterFreq = (self.freqMin + self.freqMax) / 2
      state.setActive(self.id)
      return true
    else
      local wasNavigating = self.isNavigating
      local wasDragAxis = self.dragAxis
      self.isNavigating = false
      self.dragAxis = nil
      state.setActive(nil)

      if wasNavigating and self.clickStartX and self.clickStartY then
        local dx = rx - self.clickStartX
        local dy = ry - self.clickStartY
        local dist = math.sqrt(dx * dx + dy * dy)
        if dist < 6 then
          -- Direct click-to-tune on the River!
          local clickFreq = self:getFreqAtY(self.clickStartY)
          self:tuneActiveSignalBox(clickFreq)
          return true
        elseif wasDragAxis == "vertical" then
          if self.onFrequencyChange then
            self:onFrequencyChange(self.freqMin, self.freqMax, true)
          end
        end
      end
      return true
    end
  elseif event.type == "mouseMotion" then
    local isAlt = false
    if event.modifiers then
      if event.modifiers.alt then isAlt = true end
      for _, m in ipairs(event.modifiers) do
        if m == "input.ALT" or m == "ALT" then isAlt = true end
      end
    end
    if _G.isAltDown and _G.isAltDown() then isAlt = true end
    self.isAltDown = isAlt

    self.hoverActive = true
    self.hoverX = event.x or 0
    self.hoverY = event.y or 0

    if self.isNavigating then
      local dx = (event.gx or event.x or 0) - self.dragStartX
      local dy = (event.gy or event.y or 0) - self.dragStartY
      local absDx = math.abs(dx)
      local absDy = math.abs(dy)
      local historyThreshold = self.props.historyDragThresholdPx or self.historyDragThresholdPx or 40

      -- Gesture axis locking with parametric threshold to prevent accidental history scrubbing during vertical frequency pan
      if not self.dragAxis then
        if absDx >= historyThreshold and absDx > absDy * 1.5 then
          self.dragAxis = "horizontal"
        elseif absDy >= 6 and absDy > absDx then
          self.dragAxis = "vertical"
        end
      end

      if self.dragAxis == "horizontal" then
        -- Horizontal drag scrubs time (dragging right pulls forward towards NOW, dragging left pulls into history)
        local effectiveDx = (dx > 0) and (dx - historyThreshold) or (dx + historyThreshold)
        local secPerPx = self.timeSpanSec / (self.props.w or 800)
        local newOffset = self.dragStartTimeOffset + (effectiveDx * secPerPx)
        if newOffset < 0.05 then
          newOffset = 0.0
        end
        self.timeOffsetSec = clamp(newOffset, 0.0, self.totalBufferSec - self.timeSpanSec)
      elseif self.dragAxis == "vertical" then
        -- Vertical drag pans frequency; timeOffsetSec remains strictly unchanged
        local fSpan = self.freqMax - self.freqMin
        local rulerH = self.props.rulerH or 22
        local canvasH = math.max(10, (self.props.h or 450) - rulerH)
        local hzPerPx = fSpan / canvasH
        local newCenter = self.dragStartCenterFreq + (dy * hzPerPx)
        local fMin = newCenter - fSpan / 2
        local fMax = newCenter + fSpan / 2

        -- Pin edges strictly within the receiver's active 300 kHz passband
        local hwCenter = self.hwCenterFreq or (Model.spectrumCenterFreq and Model.spectrumCenterFreq:peek()) or 14.2e6
        local bandMin = hwCenter - 150e3
        local bandMax = hwCenter + 150e3

        if fMin < bandMin then
          fMin = bandMin
          fMax = bandMin + fSpan
        end
        if fMax > bandMax then
          fMax = bandMax
          fMin = bandMax - fSpan
        end

        self.freqMin = fMin
        self.freqMax = fMax
        self:layout(self.props.x or 0, self.props.y or 0, self.props.w or 800, self.props.h or 450)
        if self.onFrequencyChange then
          self:onFrequencyChange(self.freqMin, self.freqMax, false)
        end
      end
      return true
    end
  elseif event.type == "mouseWheel" then
    return self:handleWheelZoom(event)
  end

  return false
end

function RiverView:handleWheelZoom(event)
  local isShift = (event.modifiers and event.modifiers.shift) or (_G.isShiftDown and _G.isShiftDown())

  if isShift then
    -- Shift + scroll: scrub time horizontally
    local stepSec = (event.delta > 0 and 1 or -1) * (self.timeSpanSec * 0.1)
    self.timeOffsetSec = clamp(self.timeOffsetSec + stepSec, 0.0, self.totalBufferSec - self.timeSpanSec)
    return true
  else
    -- Cursor-centered frequency zoom (strictly capped to 300 kHz passband)
    local factor = (event.delta > 0) and 0.85 or 1.15
    local ax, ay = self:getAbsolutePos()
    local gy = event.gy or (event.y and (event.y + ay)) or (ay + (self.props.h or 450) / 2)
    local localY = clamp(gy - ay, 0, self.props.h or 450)
    local cursorFreq = self:getFreqAtY(localY)
    local curSpan = self.freqMax - self.freqMin
    local newSpan = math.max(10e3, math.min(300e3, curSpan * factor))

    local hwCenter = self.hwCenterFreq or (Model.spectrumCenterFreq and Model.spectrumCenterFreq:peek()) or 14.2e6
    local bandMin = hwCenter - 150e3
    local bandMax = hwCenter + 150e3

    local ratio = clamp((cursorFreq - self.freqMin) / curSpan, 0.0, 1.0)
    local fMin = cursorFreq - (ratio * newSpan)
    local fMax = cursorFreq + ((1.0 - ratio) * newSpan)

    -- Pin zoom edges within the 300 kHz passband
    if fMin < bandMin then
      fMin = bandMin
      fMax = bandMin + newSpan
    end
    if fMax > bandMax then
      fMax = bandMax
      fMin = bandMax - newSpan
    end
    if fMin < bandMin then fMin = bandMin end

    self.freqMin = fMin
    self.freqMax = fMax
    self:layout(self.props.x or 0, self.props.y or 0, self.props.w or 800, self.props.h or 450)
    if self.onFrequencyChange then
      self:onFrequencyChange(self.freqMin, self.freqMax, true)
    end
    return true
  end
end

return RiverView
