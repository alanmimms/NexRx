--[[
  SignalBox.lua - Demodulation band widget for The River

  Represents an active demodulation channel spanning across time and
  bounded vertically by upper and lower frequency skirts, featuring
  outward-facing chevron handles and an amber playback cursor.
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

local SignalBox = Widget.mkType("SignalBox", Widget)

setbox.rule {
  id = "signalbox-defaults",
  tags = { "widget.SignalBox" },
  priority = -1000,
  apply = {
    background = "#facc15",
    border = "#fde047",
    handleColor = "#38bdf8",
    playbackColor = "#f59e0b",
    carrierColor = "#ffffff",
    opacity = 0.22,
    selectedOpacity = 0.38,
    borderWidth = 1.5,
    selectedBorderWidth = 2.5,
  }
}

function SignalBox:init(def)
  def = def or {}
  Widget.init(self, def)
  self.boxIndex = def.index or 1
  self.mode = def.mode or "USB"
  self.carrierFreq = def.carrierFreq or 14.240e6
  self.bandwidth = def.bandwidth or 2800
  self.label = def.label or "CQ"

  if self.lwc then
    self.lwc:setTag("mode." .. self.mode:upper(), true)
  end

  self.isDragging = false
  self.dragMode = nil -- "move", "topHandle", "bottomHandle"
  self.dragStartY = 0
  self.dragStartCarrier = self.carrierFreq
  self.dragStartBw = self.bandwidth

  self.playbackActive = false
  self.playbackX = 0
  self.playbackOffsetSec = 0
end

function SignalBox:setMode(newMode)
  if not newMode then return end
  local oldTag = "mode." .. tostring(self.mode):upper()
  self.mode = newMode
  local newTag = "mode." .. tostring(self.mode):upper()
  if self.lwc then
    self.lwc:setTag(oldTag, false)
    self.lwc:setTag(newTag, true)
  end
  Model.set("rx.selectedMode", self.mode)
  local river = self.parent
  if river and river.layout then
    river:layout(river.props.x or 0, river.props.y or 0, river.props.w or 800, river.props.h or 450)
  end
end

function SignalBox:cycleMode()
  local modeList = { "USB", "LSB", "CW", "AM", "FM" }
  local curIdx = 1
  for i, m in ipairs(modeList) do
    if m == self.mode then curIdx = i; break end
  end
  local nextIdx = (curIdx % #modeList) + 1
  self:setMode(modeList[nextIdx])
end

function SignalBox:onEvent(event)
  if event.type == "key" then
    if event.key == "TAB" then
      return false -- Bubble to RiverView to cycle focus
    elseif (event.key == "DELETE" or event.key == "BACKSPACE") and event.isDown then
      if self.parent and self.parent.removeFocusedSignalBox then
        return self.parent:removeFocusedSignalBox()
      end
    end
  end

  if event.type == "mouseButton" and event.button == "LEFT" then
    if event.isDown then
      local isShift = (event.modifiers and (event.modifiers.shift or (event.modifiers[1] == "input.SHIFT"))) or (_G.isShiftDown and _G.isShiftDown())
      if isShift and self.parent and self.parent.spawnSignalBox then
        local river = self.parent
        local gy = event.gy or (event.y + (self.props.y or 0))
        local clickFreq = river:getFreqAtY(gy - (river.props.y or 0))
        river:spawnSignalBox(clickFreq)
        return true
      end

      local lx = event.x or 0
      local ly = event.y or 0
      local w = self.props.w or 100
      local h = self.props.h or 10

      -- 1. Mode Badge on the left (x: 8 to 72)
      if lx >= 8 and lx <= 72 and ly >= 2 and ly <= h - 2 then
        self:cycleMode()
        Model.selectedSignalBoxIndex:set(self.boxIndex)
        self:setFocus()
        Model.set("rx.VFO.activeValue", self.carrierFreq)
        Model.set("rx.selectedMode", self.mode)
        return true
      end

      -- 2. "Always Live" zone near the right edge (lx >= w - 60): Chevron handles & live tuning
      if lx >= w - 60 then
        -- Clicking in the live zone cancels historical playback
        if self.playbackActive then
          self.playbackActive = false
          if _G.audio and _G.audio.stopPlayback then
            _G.audio.stopPlayback()
          end
        end

        if ly <= math.max(8, h * 0.45) then
          self.dragMode = "topHandle"
        elseif ly >= math.min(h - 8, h * 0.55) then
          self.dragMode = "bottomHandle"
        else
          self.dragMode = "move"
        end
      else
        -- 3. Historical Audio Area: Strictly to the left of the live zone and CAD break
        local river = self.parent
        local historyW = w
        local tOffset = (river and river.timeOffsetSec) or 0.0
        local tSpan = (river and river.timeSpanSec) or 30.0
        if river and river.timeOffsetSec and river.timeOffsetSec > 0.01 then
          local breakW = 24
          local pinnedW = (river.props and river.props.pinnedSliverW) or 60
          historyW = math.max(20, w - breakW - pinnedW)
        end

        local maxHistClickX = math.min(historyW - 15, w - 60)
        if lx >= 75 and lx <= maxHistClickX then
          local normX = clamp(lx / historyW, 0.0, 1.0)
          local offsetSec = tOffset + (1.0 - normX) * tSpan
          self.playbackActive = true
          self.playbackX = lx
          self.playbackOffsetSec = offsetSec
          if _G.audio and _G.audio.seekPlayback then
            _G.audio.seekPlayback(offsetSec)
          end
        end

        self.dragMode = "move"
      end

      local bw = self.bandwidth or 2800
      local cFreq = self.carrierFreq or 14.2e6
      local lowFreq, highFreq
      if self.mode == "USB" then
        lowFreq = cFreq
        highFreq = cFreq + bw
      elseif self.mode == "LSB" then
        lowFreq = cFreq - bw
        highFreq = cFreq
      else
        lowFreq = cFreq - bw / 2
        highFreq = cFreq + bw / 2
      end
      self.dragStartLowFreq = lowFreq
      self.dragStartHighFreq = highFreq
      self.dragStartCarrier = cFreq
      self.dragStartBw = bw

      self.isDragging = true
      self.dragStartY = event.gy or event.y or 0

      state.setActive(self.id)
      Model.selectedSignalBoxIndex:set(self.boxIndex)
      self:setFocus()
      Model.set("rx.VFO.activeValue", self.carrierFreq)
      Model.set("rx.selectedMode", self.mode)
      return true
    else
      self.isDragging = false
      self.dragMode = nil
      state.setActive(nil)
      return true
    end
  elseif event.type == "mouseMotion" and self.isDragging then
    local dy = (event.gy or event.y or 0) - self.dragStartY
    local river = self.parent
    local hzPerPx = 100
    if river and river.freqMax and river.freqMin then
      local canvasH = math.max(10, (river.props.h or 450) - (river.props.rulerH or 22))
      hzPerPx = (river.freqMax - river.freqMin) / canvasH
    end

    if self.dragMode == "move" then
      -- Dragging up increases frequency; 1:1 on the River canvas
      local newFreq = self.dragStartCarrier - (dy * hzPerPx)
      if river and river.freqMin and river.freqMax then
        newFreq = math.max(river.freqMin, math.min(river.freqMax, newFreq))
      end
      self.carrierFreq = newFreq
      Model.set("rx.VFO.activeValue", self.carrierFreq)
    elseif self.dragMode == "topHandle" then
      -- Vernier scaling for chevrons: 25 Hz per pixel for fine, controllable filter adjustments
      local vernierHzPerPx = 25.0
      local deltaHz = (dy * vernierHzPerPx)
      local newHighFreq = math.max(self.dragStartLowFreq + 200, math.min(self.dragStartLowFreq + 16000, self.dragStartHighFreq - deltaHz))
      if self.mode == "USB" then
        self.bandwidth = math.max(200, math.min(16000, math.floor(newHighFreq - self.dragStartLowFreq)))
      elseif self.mode == "LSB" then
        self.carrierFreq = newHighFreq
        self.bandwidth = math.max(200, math.min(16000, math.floor(newHighFreq - self.dragStartLowFreq)))
        Model.set("rx.VFO.activeValue", self.carrierFreq)
      else
        self.carrierFreq = math.floor((newHighFreq + self.dragStartLowFreq) / 2)
        self.bandwidth = math.max(200, math.min(16000, math.floor(newHighFreq - self.dragStartLowFreq)))
        Model.set("rx.VFO.activeValue", self.carrierFreq)
      end
      Model.set("rx.bandpass.width", self.bandwidth)
    elseif self.dragMode == "bottomHandle" then
      -- Vernier scaling for chevrons: 25 Hz per pixel for fine, controllable filter adjustments
      local vernierHzPerPx = 25.0
      local deltaHz = (dy * vernierHzPerPx)
      local newLowFreq = math.min(self.dragStartHighFreq - 200, math.max(self.dragStartHighFreq - 16000, self.dragStartLowFreq - deltaHz))
      if self.mode == "USB" then
        self.carrierFreq = newLowFreq
        self.bandwidth = math.max(200, math.min(16000, math.floor(self.dragStartHighFreq - newLowFreq)))
        Model.set("rx.VFO.activeValue", self.carrierFreq)
      elseif self.mode == "LSB" then
        self.bandwidth = math.max(200, math.min(16000, math.floor(self.dragStartHighFreq - newLowFreq)))
      else
        self.carrierFreq = math.floor((self.dragStartHighFreq + newLowFreq) / 2)
        self.bandwidth = math.max(200, math.min(16000, math.floor(self.dragStartHighFreq - newLowFreq)))
        Model.set("rx.VFO.activeValue", self.carrierFreq)
      end
      Model.set("rx.bandpass.width", self.bandwidth)
    end

    -- Live visual update of SignalBox height and position on the River canvas
    if river and river.layout then
      river:layout(river.props.x or 0, river.props.y or 0, river.props.w or 800, river.props.h or 450)
    end
    return true
  elseif event.type == "mouseWheel" then
    local river = self.parent
    if river and river.handleWheelZoom then
      return river:handleWheelZoom(event)
    end
  end

  return Widget.onEvent(self, event)
end

function SignalBox:drawSelf()
  local w, h = self.props.w or 0, self.props.h or 0
  if w <= 0 or h <= 0 then return end

  local isSelected = (Model.selectedSignalBoxIndex:get() == self.boxIndex)
  if self.lwc then
    self.lwc:setTag("state.Selected", isSelected)
  end

  local bgR, bgG, bgB = state.hexToRgb(self.props.background or "#facc15")
  local bR, bG, bB = state.hexToRgb(self.props.border or "#fde047")
  local alpha = isSelected and (self.props.selectedOpacity or 0.38) or (self.props.opacity or 0.22)
  local bWidth = isSelected and (self.props.selectedBorderWidth or 2.5) or (self.props.borderWidth or 1.5)

  -- Passband fill
  System.drawRect(0, 0, w, h, { bgR, bgG, bgB, alpha })

  -- Upper and lower skirt boundary lines
  System.drawLine(0, 0, w, 0, bWidth, { bR, bG, bB, 0.9 })
  System.drawLine(0, h, w, h, bWidth, { bR, bG, bB, 0.9 })

  -- Virtual carrier frequency line
  local carrierY = (self.mode == "USB") and h or ((self.mode == "LSB") and 0 or (h / 2))
  System.drawLine(0, carrierY, w, carrierY, 1.5, { 1.0, 1.0, 1.0, 0.75 })

  -- Interactive Chevrons & Handles (prominently rendered on focused SignalBox)
  if isSelected then
    local hR, hG, hB = state.hexToRgb(self.props.handleColor or "#38bdf8")
    local hx = w - 42

    -- Top chevron (pointing upward / outward)
    System.drawRect(hx - 2, 0, 36, 6, { hR, hG, hB, 0.4 })
    System.drawLine(hx, 5, hx + 16, 1, 2.5, { hR, hG, hB, 0.95 })
    System.drawLine(hx + 16, 1, hx + 32, 5, 2.5, { hR, hG, hB, 0.95 })

    -- Bottom chevron (pointing downward / outward)
    System.drawRect(hx - 2, h - 6, 36, 6, { hR, hG, hB, 0.4 })
    System.drawLine(hx, h - 5, hx + 16, h - 1, 2.5, { hR, hG, hB, 0.95 })
    System.drawLine(hx + 16, h - 1, hx + 32, h - 5, 2.5, { hR, hG, hB, 0.95 })
  end

  -- Mode Badge on the left (clickable to cycle mode)
  local modeBadgeW = 54
  local badgeH = math.min(22, math.max(14, h - 4))
  local badgeY = clamp(h / 2 - badgeH / 2, 2, h - badgeH - 2)
  local badgeBg = isSelected and { 0.1, 0.15, 0.25, 0.85 } or { 0.05, 0.08, 0.15, 0.75 }
  System.drawRoundedRect(8, badgeY, modeBadgeW, badgeH, 3, badgeBg)
  System.drawRectLines(8, badgeY, modeBadgeW, badgeH, 1.0, { bR, bG, bB, 0.7 })

  local modeStr = tostring(self.mode)
  local mw = System.measureText(modeStr, 12) or (#modeStr * 8)
  System.drawText(modeStr, 8 + (modeBadgeW - mw) / 2, badgeY + (badgeH - 12) / 2, 12, { 1.0, 1.0, 1.0, 0.95 })

  -- Label & Bandwidth: e.g. "CQ W1AW | 2.8 kHz"
  local bwKhz = string.format("%.1f kHz", self.bandwidth / 1000)
  local labelText = string.format("%s  (%s)", tostring(self.label), bwKhz)
  System.drawText(labelText, 70, clamp(h / 2 - 6, 2, h - 14), 12, { 0.95, 0.95, 0.95, 0.9 })

  -- Amber Playback Cursor (if time-machine playback is active)
  if _G.audio and _G.audio.isPlaybackActive and isSelected then
    local active = _G.audio.isPlaybackActive()
    if active then
      self.playbackActive = true
      local curOffset = (_G.audio.getPlaybackOffset and _G.audio.getPlaybackOffset()) or 0.0
      self.playbackOffsetSec = curOffset
      local river = self.parent
      local historyW = w
      local tOffset = (river and river.timeOffsetSec) or 0.0
      local tSpan = (river and river.timeSpanSec) or 30.0
      if river and river.timeOffsetSec and river.timeOffsetSec > 0.01 then
        local breakW = 24
        local pinnedW = (river.props and river.props.pinnedSliverW) or 60
        historyW = math.max(20, w - breakW - pinnedW)
      end
      local norm = 1.0 - clamp((curOffset - tOffset) / tSpan, 0.0, 1.0)
      self.playbackX = norm * historyW
    else
      self.playbackActive = false
    end
  end

  if self.playbackActive and self.playbackX > 0 and self.playbackX < w then
    local pbR, pbG, pbB = state.hexToRgb(self.props.playbackColor or "#f59e0b")
    local px = self.playbackX

    -- 1. Full height vertical amber playback line
    System.drawLine(px, 0, px, h, 2.0, { pbR, pbG, pbB, 1.0 })

    -- 2. Outward chevrons: (v) at top skirt, (^) at bottom skirt, (|) at carrier line
    System.drawLine(px - 5, -5, px, 0, 2.0, { pbR, pbG, pbB, 1.0 })
    System.drawLine(px + 5, -5, px, 0, 2.0, { pbR, pbG, pbB, 1.0 })

    System.drawLine(px - 5, h + 5, px, h, 2.0, { pbR, pbG, pbB, 1.0 })
    System.drawLine(px + 5, h + 5, px, h, 2.0, { pbR, pbG, pbB, 1.0 })

    System.drawCircle(px, carrierY, 3.5, { pbR, pbG, pbB, 1.0 })

    -- 3. Scoped Tektronix phosphor-green mini-trace riding directly on playback cursor
    local traceHalfW = 16
    local traceYMid = h / 2
    local traceAmp = math.min(h * 0.35, 12)
    local tPoints = 14
    local lastTx, lastTy
    local phase = (self.playbackOffsetSec or 0.0) * 12.0
    for pt = 0, tPoints do
      local tNorm = pt / tPoints
      local tx = px - traceHalfW + (tNorm * traceHalfW * 2)
      local wave = math.sin(tNorm * 4.0 * math.pi + phase) * math.cos(tNorm * 2.0 * math.pi)
      local ty = traceYMid + (wave * traceAmp)
      if lastTx then
        System.drawLine(lastTx, lastTy, tx, ty, 1.5, { 0.25, 0.95, 0.45, 0.85 })
      end
      lastTx, lastTy = tx, ty
    end

    -- 4. Instantaneous single-bar S-meter alongside the cursor
    local smeterW = 3
    local smeterH = math.max(8, math.min(h - 4, 20))
    local smeterY = clamp(carrierY - smeterH / 2, 2, h - smeterH - 2)
    System.drawRect(px + 5, smeterY, smeterW, smeterH, { 0.1, 0.2, 0.15, 0.7 })
    System.drawRect(px + 5, smeterY + smeterH * 0.35, smeterW, smeterH * 0.65, { 0.3, 0.9, 0.4, 0.9 })
  end
end

return SignalBox
