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

  self.isDragging = false
  self.dragMode = nil -- "move", "topHandle", "bottomHandle"
  self.dragStartY = 0
  self.dragStartCarrier = self.carrierFreq
  self.dragStartBw = self.bandwidth

  self.playbackActive = false
  self.playbackX = 0
end

function SignalBox:onEvent(event)
  if event.type == "mouseButton" and event.button == "LEFT" then
    if event.isDown then
      local ly = event.y or 0
      local h = self.props.h or 10

      if ly <= 6 then
        self.dragMode = "topHandle"
      elseif ly >= h - 6 then
        self.dragMode = "bottomHandle"
      else
        self.dragMode = "move"
      end

      self.isDragging = true
      self.dragStartY = event.gy or event.y or 0
      self.dragStartCarrier = self.carrierFreq
      self.dragStartBw = self.bandwidth

      state.setActive(self.id)
      Model.selectedSignalBoxIndex:set(self.boxIndex)
      self:setFocus()
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
    if river and river.props and river.props.h and river.props.freqMax and river.props.freqMin then
      hzPerPx = (river.props.freqMax - river.props.freqMin) / river.props.h
    end

    if self.dragMode == "move" then
      -- Dragging up increases frequency
      self.carrierFreq = self.dragStartCarrier - (dy * hzPerPx)
      Model.set("rx.VFO.activeValue", self.carrierFreq)
    elseif self.dragMode == "topHandle" then
      -- Top handle: adjust upper skirt
      local deltaBw = -(dy * hzPerPx)
      self.bandwidth = math.max(200, self.dragStartBw + deltaBw)
    elseif self.dragMode == "bottomHandle" then
      -- Bottom handle: adjust lower skirt
      local deltaBw = (dy * hzPerPx)
      self.bandwidth = math.max(200, self.dragStartBw + deltaBw)
    end
    return true
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
  System.drawLine(0, 0, w, 0, bWidth, { bR, bG, bB, 0.85 })
  System.drawLine(0, h, w, h, bWidth, { bR, bG, bB, 0.85 })

  -- Virtual carrier frequency line (dashed or subtle solid)
  local carrierY = (self.mode == "USB") and h or ((self.mode == "LSB") and 0 or (h / 2))
  System.drawLine(0, carrierY, w, carrierY, 1.0, { 1.0, 1.0, 1.0, 0.6 })

  -- Outward-facing bracket / chevron handles (only on focused SignalBox)
  if isSelected then
    local hR, hG, hB = state.hexToRgb(self.props.handleColor or "#38bdf8")
    local hx = w - 18
    -- Top chevron (v) sitting above upper skirt
    System.drawLine(hx, -4, hx + 5, -1, 2.0, { hR, hG, hB, 0.95 })
    System.drawLine(hx + 5, -1, hx + 10, -4, 2.0, { hR, hG, hB, 0.95 })

    -- Bottom chevron (^) sitting below lower skirt
    System.drawLine(hx, h + 4, hx + 5, h + 1, 2.0, { hR, hG, hB, 0.95 })
    System.drawLine(hx + 5, h + 1, hx + 10, h + 4, 2.0, { hR, hG, hB, 0.95 })
  end

  -- Label: e.g. "CQ W1AW (USB)"
  local tagText = string.format("%s (%s)", tostring(self.label), tostring(self.mode))
  System.drawText(tagText, 10, clamp(h / 2 - 7, 2, h - 16), 13, { 1.0, 1.0, 1.0, 0.95 })

  -- Amber Playback Cursor (if time-machine playback is active)
  if self.playbackActive and self.playbackX > 0 and self.playbackX < w then
    local pbR, pbG, pbB = state.hexToRgb(self.props.playbackColor or "#f59e0b")
    System.drawLine(self.playbackX, -6, self.playbackX, h + 6, 2.0, { pbR, pbG, pbB, 1.0 })
    -- Scoped diamond head
    System.drawCircle(self.playbackX, carrierY, 3.5, { pbR, pbG, pbB, 1.0 })
  end
end

return SignalBox
