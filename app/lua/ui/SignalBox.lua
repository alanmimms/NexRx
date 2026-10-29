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

local function getModifiers(event)
  local isShift = false
  local isCtrl = false
  local isAlt = false
  if event and event.modifiers then
    if event.modifiers.shift or event.modifiers.SHIFT then isShift = true end
    if event.modifiers.ctrl or event.modifiers.CTRL or event.modifiers.control or event.modifiers.CONTROL then isCtrl = true end
    if event.modifiers.alt or event.modifiers.ALT then isAlt = true end
    if type(event.modifiers) == "table" then
      for _, mod in ipairs(event.modifiers) do
        if mod == "input.SHIFT" or mod == "shift" or mod == "SHIFT" then isShift = true end
        if mod == "input.CTRL" or mod == "ctrl" or mod == "CTRL" or mod == "control" then isCtrl = true end
        if mod == "input.ALT" or mod == "alt" or mod == "ALT" then isAlt = true end
      end
    end
  end
  if _G.isShiftDown and _G.isShiftDown() then isShift = true end
  if _G.isCtrlDown and _G.isCtrlDown() then isCtrl = true end
  if _G.isAltDown and _G.isAltDown() then isAlt = true end
  return isShift, isCtrl, isAlt
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

  self.callerCallsign = def.caller
  self.respondentCallsign = def.respondent
  self.isLabelLocked = def.locked or false

  if def.label then
    self.label = def.label
    if not self.callerCallsign then
      local extracted = self.label:match("CQ%s+([%w/]+)") or self.label:match("^([%w/]+)%s*/")
      if extracted and #extracted >= 3 and not extracted:match("^BOX") then
        self.callerCallsign = extracted
      end
    end
  elseif self.callerCallsign and self.respondentCallsign then
    self.label = string.format("%s / %s", self.callerCallsign, self.respondentCallsign)
  elseif self.callerCallsign then
    self.label = string.format("CQ %s", self.callerCallsign)
  else
    self.label = "CQ"
  end

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
  self.hoverActive = false
  self.hoverX = 0
  self.hoverY = 0
end

function SignalBox:contains(x, y)
  local w = self.props.w or 0
  local h = self.props.h or 0
  if x < 0 or x >= w then return false end

  -- Inside normal bounding box
  if y >= 0 and y < h then return true end

  -- In chevron handle region near right edge (x >= w - 60):
  -- Top chevron extends above the top: y in [-15, 0]
  -- Bottom chevron extends below the bottom: y in [h, h + 15]
  local chevronH = 15
  if x >= w - 60 then
    if y >= -chevronH and y <= h + chevronH then
      return true
    end
  end

  return false
end

function SignalBox:stopPlayback()
  if self.playbackActive then
    self.playbackActive = false
    self.playbackX = 0
    self.playbackOffsetSec = 0
    if _G.audio and _G.audio.stopPlayback then
      _G.audio.stopPlayback()
    end
  end
end

function SignalBox:onBlur()
  self:stopPlayback()
end

function SignalBox:updateQSOState(caller, respondent)
  if caller then self.callerCallsign = caller end
  if respondent then self.respondentCallsign = respondent end
  if not self.isLabelLocked then
    if self.callerCallsign and self.respondentCallsign then
      self.label = string.format("%s / %s", self.callerCallsign, self.respondentCallsign)
    elseif self.callerCallsign then
      self.label = string.format("CQ %s", self.callerCallsign)
    end
  end
end

function SignalBox:lockCustomLabel(name)
  if name and #name > 0 then
    self.label = name
    self.isLabelLocked = true
  end
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
      local isShift, isCtrl, isAlt = getModifiers(event)
      if (isAlt or (self.parent and self.parent.isAltDown)) and self.parent and self.parent.spawnSignalBox then
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
      if lx >= 8 and lx <= 72 and ly >= math.min(0, h / 2 - 10) and ly <= math.max(h, h / 2 + 10) then
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
        self:stopPlayback()

        local isTopChevron = (ly >= -15 and ly <= 0)
        local isBottomChevron = (ly >= h and ly <= h + 15)

        if isTopChevron then
          self.dragMode = "topHandle"
        elseif isBottomChevron then
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
          local breakW = (river.props and river.props.cadBreakW) or 0
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
      self.lastDragY = self.dragStartY
      self.dragActiveDelta = nil
      self.dragAccumPx = 0
      if self.dragMode == "topHandle" then
        self.currentDragFreq = self.dragStartHighFreq
      elseif self.dragMode == "bottomHandle" then
        self.currentDragFreq = self.dragStartLowFreq
      else
        self.currentDragFreq = self.dragStartCarrier
      end

      state.setActive(self.id)
      Model.selectedSignalBoxIndex:set(self.boxIndex)
      self:setFocus()
      Model.set("rx.VFO.activeValue", self.carrierFreq)
      Model.set("rx.selectedMode", self.mode)
      return true
    else
      self.isDragging = false
      self.dragMode = nil
      self.dragActiveDelta = nil
      self.dragAccumPx = 0
      state.setActive(nil)
      return true
    end
  elseif event.type == "mouseMotion" then
    self.hoverActive = true
    self.hoverX = event.x or 0
    self.hoverY = event.y or 0
    if not self.isDragging then
      return false
    end
    local dy = (event.gy or event.y or 0) - self.dragStartY
    local river = self.parent
    local hzPerPx = 100
    if river and river.freqMax and river.freqMin then
      local canvasH = math.max(10, (river.props.h or 450) - (river.props.rulerH or 22))
      hzPerPx = (river.freqMax - river.freqMin) / canvasH
    end

    if self.dragMode == "move" then
      local curY = event.gy or event.y or 0
      local stepY = curY - (self.lastDragY or curY)

      local isShift, isCtrl = getModifiers(event)
      local delta = nil
      if isShift and isCtrl then
        delta = 10000 -- 10 kHz
      elseif isShift then
        delta = 1000  -- 1 kHz
      elseif isCtrl then
        delta = 100   -- 100 Hz
      end

      if delta then
        if self.dragActiveDelta ~= delta then
          if stepY ~= 0 then
            local dir = (stepY < 0) and 1 or -1 -- UP (negative stepY) increases frequency
            local F = self.currentDragFreq or self.dragStartCarrier
            local snapped
            if dir > 0 then
              if F % delta == 0 then
                snapped = F + delta
              else
                snapped = math.ceil(F / delta) * delta
              end
            else
              if F % delta == 0 then
                snapped = F - delta
              else
                snapped = math.floor(F / delta) * delta
              end
            end
            self.currentDragFreq = snapped
            self.dragActiveDelta = delta
            self.dragAccumPx = 0
            self.lastDragY = curY
          end
        else
          self.dragAccumPx = (self.dragAccumPx or 0) + stepY
          self.lastDragY = curY
          local pxPerStep = 8.0
          if self.dragAccumPx <= -pxPerStep then
            local nSteps = math.floor((-self.dragAccumPx) / pxPerStep)
            self.currentDragFreq = self.currentDragFreq + (nSteps * delta)
            self.dragAccumPx = self.dragAccumPx + (nSteps * pxPerStep)
          elseif self.dragAccumPx >= pxPerStep then
            local nSteps = math.floor(self.dragAccumPx / pxPerStep)
            self.currentDragFreq = self.currentDragFreq - (nSteps * delta)
            self.dragAccumPx = self.dragAccumPx - (nSteps * pxPerStep)
          end
        end

        local newFreq = self.currentDragFreq
        if river and river.freqMin and river.freqMax then
          newFreq = math.max(river.freqMin, math.min(river.freqMax, newFreq))
        end
        self.carrierFreq = newFreq
        if river and river.updateBoxMetadataForFreq then
          river:updateBoxMetadataForFreq(self, self.carrierFreq)
        end
        Model.set("rx.VFO.activeValue", self.carrierFreq)
      else
        if self.dragActiveDelta ~= nil then
          self.dragStartCarrier = self.currentDragFreq or self.carrierFreq
          self.dragStartY = curY
          self.dragActiveDelta = nil
          self.dragAccumPx = 0
        end

        self.lastDragY = curY
        local newFreq = self.dragStartCarrier - (dy * hzPerPx)
        if river and river.freqMin and river.freqMax then
          newFreq = math.max(river.freqMin, math.min(river.freqMax, newFreq))
        end
        self.currentDragFreq = newFreq
        self.carrierFreq = newFreq
        if river and river.updateBoxMetadataForFreq then
          river:updateBoxMetadataForFreq(self, self.carrierFreq)
        end
        Model.set("rx.VFO.activeValue", self.carrierFreq)
      end
    elseif self.dragMode == "topHandle" or self.dragMode == "bottomHandle" then
      local isTop = (self.dragMode == "topHandle")
      local curY = event.gy or event.y or 0
      local stepY = curY - (self.lastDragY or curY)

      local isShift, isCtrl = getModifiers(event)
      local delta = nil
      if isShift and isCtrl then
        delta = 10000 -- 10 kHz
      elseif isShift then
        delta = 1000  -- 1 kHz
      elseif isCtrl then
        delta = 100   -- 100 Hz
      end

      local minBw = 200
      local maxBw = 100000

      if delta then
        if self.dragActiveDelta ~= delta then
          -- Modifier key was pressed or changed (even while mouse is already down).
          -- "the next motion event moves to the nearest multiple of the delta
          -- in the direction of the motion and stays on integral multiples of that delta during subsequent motion."
          if stepY ~= 0 then
            local dir = (stepY < 0) and 1 or -1 -- UP (negative stepY) increases frequency
            local F = self.currentDragFreq or (isTop and self.dragStartHighFreq or self.dragStartLowFreq)
            local snapped
            if dir > 0 then
              if F % delta == 0 then
                snapped = F + delta
              else
                snapped = math.ceil(F / delta) * delta
              end
            else
              if F % delta == 0 then
                snapped = F - delta
              else
                snapped = math.floor(F / delta) * delta
              end
            end
            self.currentDragFreq = snapped
            self.dragActiveDelta = delta
            self.dragAccumPx = 0
            self.lastDragY = curY
          end
        else
          -- Staying on integral multiples of that delta during subsequent motion
          self.dragAccumPx = (self.dragAccumPx or 0) + stepY
          self.lastDragY = curY
          local pxPerStep = 8.0
          if self.dragAccumPx <= -pxPerStep then
            local nSteps = math.floor((-self.dragAccumPx) / pxPerStep)
            self.currentDragFreq = self.currentDragFreq + (nSteps * delta)
            self.dragAccumPx = self.dragAccumPx + (nSteps * pxPerStep)
          elseif self.dragAccumPx >= pxPerStep then
            local nSteps = math.floor(self.dragAccumPx / pxPerStep)
            self.currentDragFreq = self.currentDragFreq - (nSteps * delta)
            self.dragAccumPx = self.dragAccumPx - (nSteps * pxPerStep)
          end
        end

        if isTop then
          local minAllowedHigh = math.ceil((self.dragStartLowFreq + minBw) / delta) * delta
          local maxAllowedHigh = math.floor((self.dragStartLowFreq + maxBw) / delta) * delta
          self.currentDragFreq = math.max(minAllowedHigh, math.min(maxAllowedHigh, self.currentDragFreq))
          local newHighFreq = self.currentDragFreq

          if self.mode == "USB" then
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(newHighFreq - self.dragStartLowFreq)))
          elseif self.mode == "LSB" then
            self.carrierFreq = newHighFreq
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(newHighFreq - self.dragStartLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          else
            self.carrierFreq = math.floor((newHighFreq + self.dragStartLowFreq) / 2)
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(newHighFreq - self.dragStartLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          end
          Model.set("rx.bandpass.width", self.bandwidth)
        else
          local minAllowedLow = math.ceil((self.dragStartHighFreq - maxBw) / delta) * delta
          local maxAllowedLow = math.floor((self.dragStartHighFreq - minBw) / delta) * delta
          self.currentDragFreq = math.max(minAllowedLow, math.min(maxAllowedLow, self.currentDragFreq))
          local newLowFreq = self.currentDragFreq

          if self.mode == "USB" then
            self.carrierFreq = newLowFreq
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(self.dragStartHighFreq - newLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          elseif self.mode == "LSB" then
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(self.dragStartHighFreq - newLowFreq)))
          else
            self.carrierFreq = math.floor((self.dragStartHighFreq + newLowFreq) / 2)
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(self.dragStartHighFreq - newLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          end
          Model.set("rx.bandpass.width", self.bandwidth)
        end
      else
        -- Continuous 1:1 mouse cursor tracking (no modifier held)
        if self.dragActiveDelta ~= nil then
          if isTop then
            self.dragStartHighFreq = self.currentDragFreq or self.dragStartHighFreq
          else
            self.dragStartLowFreq = self.currentDragFreq or self.dragStartLowFreq
          end
          self.dragStartY = curY
          self.dragActiveDelta = nil
          self.dragAccumPx = 0
        end

        self.lastDragY = curY
        local vernierHzPerPx = hzPerPx
        local deltaHz = ((curY - self.dragStartY) * vernierHzPerPx)

        if isTop then
          local newHighFreq = math.max(self.dragStartLowFreq + minBw, math.min(self.dragStartLowFreq + maxBw, self.dragStartHighFreq - deltaHz))
          self.currentDragFreq = newHighFreq
          if self.mode == "USB" then
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(newHighFreq - self.dragStartLowFreq)))
          elseif self.mode == "LSB" then
            self.carrierFreq = newHighFreq
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(newHighFreq - self.dragStartLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          else
            self.carrierFreq = math.floor((newHighFreq + self.dragStartLowFreq) / 2)
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(newHighFreq - self.dragStartLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          end
          Model.set("rx.bandpass.width", self.bandwidth)
        else
          local newLowFreq = math.min(self.dragStartHighFreq - minBw, math.max(self.dragStartHighFreq - maxBw, self.dragStartLowFreq - deltaHz))
          self.currentDragFreq = newLowFreq
          if self.mode == "USB" then
            self.carrierFreq = newLowFreq
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(self.dragStartHighFreq - newLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          elseif self.mode == "LSB" then
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(self.dragStartHighFreq - newLowFreq)))
          else
            self.carrierFreq = math.floor((self.dragStartHighFreq + newLowFreq) / 2)
            self.bandwidth = math.max(minBw, math.min(maxBw, math.floor(self.dragStartHighFreq - newLowFreq)))
            Model.set("rx.VFO.activeValue", self.carrierFreq)
          end
          Model.set("rx.bandpass.width", self.bandwidth)
        end
      end
    end

    -- Live visual update of SignalBox height and position on the River canvas
    if river and river.layout then
      river:layout(river.props.x or 0, river.props.y or 0, river.props.w or 800, river.props.h or 450)
    end
    if river and river.onSignalBoxesChanged then
      river:onSignalBoxesChanged(river.signalBoxes)
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
  if not isSelected and self.playbackActive then
    self:stopPlayback()
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

    -- Top chevron (fixed 15px area extending upward from top of signalbox y=0 to y=-15)
    System.drawRect(hx - 2, -15, 36, 15, { hR, hG, hB, 0.35 })
    System.drawRectLines(hx - 2, -15, 36, 15, 1.0, { hR, hG, hB, 0.75 })
    System.drawLine(hx + 2, -4, hx + 16, -12, 2.5, { hR, hG, hB, 0.98 })
    System.drawLine(hx + 16, -12, hx + 30, -4, 2.5, { hR, hG, hB, 0.98 })

    -- Bottom chevron (fixed 15px area extending downward from bottom of signalbox y=h to y=h+15)
    System.drawRect(hx - 2, h, 36, 15, { hR, hG, hB, 0.35 })
    System.drawRectLines(hx - 2, h, 36, 15, 1.0, { hR, hG, hB, 0.75 })
    System.drawLine(hx + 2, h + 4, hx + 16, h + 12, 2.5, { hR, hG, hB, 0.98 })
    System.drawLine(hx + 16, h + 12, hx + 30, h + 4, 2.5, { hR, hG, hB, 0.98 })
  end

  -- Mode Badge on the left (clickable to cycle mode)
  local modeBadgeW = 54
  local badgeH = math.min(20, math.max(14, h - 2))
  local badgeY = h / 2 - badgeH / 2
  local badgeBg = isSelected and { 0.1, 0.15, 0.25, 0.85 } or { 0.05, 0.08, 0.15, 0.75 }
  System.drawRoundedRect(8, badgeY, modeBadgeW, badgeH, 3, badgeBg)
  System.drawRectLines(8, badgeY, modeBadgeW, badgeH, 1.0, { bR, bG, bB, 0.7 })

  local modeStr = tostring(self.mode)
  local mw = System.measureText(modeStr, 12) or (#modeStr * 8)
  System.drawText(modeStr, 8 + (modeBadgeW - mw) / 2, badgeY + (badgeH - 12) / 2, 12, { 1.0, 1.0, 1.0, 0.95 })

  -- Label & Bandwidth: e.g. "CQ W1AW | 2.8 kHz"
  local bwKhz = string.format("%.1f kHz", self.bandwidth / 1000)
  local labelText = string.format("%s  (%s)", tostring(self.label), bwKhz)
  System.drawText(labelText, 70, h / 2 - 6, 12, { 0.95, 0.95, 0.95, 0.9 })

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
        local breakW = (river.props and river.props.cadBreakW) or 0
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

  -- HamDB Station & Award Tooltip Card on Hover
  local isHoveringLabel = (self.isMouseOver or self.hoverActive) and (self.hoverX >= 8 and self.hoverX <= 260)
  if isHoveringLabel then
    local HamDB = require("HamDB")
    local primaryCall = self.callerCallsign or self.label
    local st1 = HamDB.lookup(primaryCall)
    if st1 then
      local awards1 = HamDB.evaluateAwards(st1, _G.operatorWorkedLog)
      if self.lwc then
        for _, aw in ipairs(awards1) do
          self.lwc:setTag(aw, true)
        end
      end

      local st2 = self.respondentCallsign and HamDB.lookup(self.respondentCallsign)
      local cardW = 230
      local cardH = st2 and 124 or 74
      local cardX = clamp(self.hoverX + 12, 10, w - cardW - 10)
      local cardY = (h > 60) and 4 or (h + 4)

      System.drawRoundedRect(cardX, cardY, cardW, cardH, 4, { 0.06, 0.09, 0.16, 0.96 })
      System.drawRectLines(cardX, cardY, cardW, cardH, 1.0, { 0.25, 0.35, 0.5, 0.85 })

      local header1 = string.format("%s - %s", st1.callsign, st1.name)
      System.drawText(header1, cardX + 8, cardY + 6, 12, { 1.0, 1.0, 1.0, 0.98 })

      local loc1 = st1.country
      if st1.city and #st1.city > 0 and st1.state and #st1.state > 0 then
        loc1 = string.format("%s, %s, %s", st1.city, st1.state, st1.country)
      elseif st1.state and #st1.state > 0 then
        loc1 = string.format("%s, %s", st1.state, st1.country)
      elseif st1.city and #st1.city > 0 then
        loc1 = string.format("%s, %s", st1.city, st1.country)
      end
      System.drawText(loc1, cardX + 8, cardY + 22, 11, { 0.75, 0.8, 0.9, 0.85 })

      local rfStr1 = string.format("Grid: %s | %d deg | %d mi (%d km)", st1.grid, st1.bearing, st1.distMi, st1.distKm)
      System.drawText(rfStr1, cardX + 8, cardY + 38, 10, { 0.38, 0.82, 0.95, 0.9 })

      if #awards1 > 0 then
        local awardNames = {}
        for _, a in ipairs(awards1) do
          table.insert(awardNames, "[" .. a:gsub("award%.", "") .. "]")
        end
        local awardStr = table.concat(awardNames, " ")
        System.drawText(awardStr, cardX + 8, cardY + 54, 10, { 0.95, 0.8, 0.3, 0.95 })
      end

      if st2 then
        System.drawLine(cardX + 6, cardY + 66, cardX + cardW - 6, cardY + 66, 1.0, { 0.2, 0.3, 0.4, 0.7 })
        local header2 = string.format("%s - %s", st2.callsign, st2.name)
        System.drawText(header2, cardX + 8, cardY + 72, 12, { 1.0, 1.0, 1.0, 0.98 })

        local loc2 = st2.country
        if st2.city and #st2.city > 0 and st2.state and #st2.state > 0 then
          loc2 = string.format("%s, %s, %s", st2.city, st2.state, st2.country)
        elseif st2.state and #st2.state > 0 then
          loc2 = string.format("%s, %s", st2.state, st2.country)
        elseif st2.city and #st2.city > 0 then
          loc2 = string.format("%s, %s", st2.city, st2.country)
        end
        System.drawText(loc2, cardX + 8, cardY + 88, 11, { 0.75, 0.8, 0.9, 0.85 })

        local rfStr2 = string.format("Grid: %s | %d deg | %d mi (%d km)", st2.grid, st2.bearing, st2.distMi, st2.distKm)
        System.drawText(rfStr2, cardX + 8, cardY + 104, 10, { 0.38, 0.82, 0.95, 0.9 })
      end
    end
  end
end

return SignalBox
