local setbox = require("SetBox")
local Widget = require("ui.Widget")
local state = require("ui.State")
local Model = require("Model")
local Hardware = require("Hardware")
local SignalBox = require("ui.SignalBox")

--[[
  GraticuleLegend Widget
  Displays scaling info (e.g. "10 kHz/div") in corners of plot widgets.
]]
local GraticuleLegend = Widget.mkType("GraticuleLegend", Widget)

-- Default rules
setbox.rule {
    id = "graticule-defaults",
    tags = {"widget.GraticuleLegend"},
    priority = -1000,
    apply = {
        background = "#000000",
        foreground = "#ffffff",
        opacity = 0.5,
        borderRadius = 4,
        padding = 4,
    }
}

function GraticuleLegend.new(id, parent)
    return GraticuleLegend({ id = id or "spec-legend", parent = parent })
end

function GraticuleLegend:draw(id, x, y, w, h, parentLWC, line1, line2)
    if parentLWC and self.lwc and self.lwc.parent ~= parentLWC then
        self.lwc.parent = parentLWC
    end

    local bgR, bgG, bgB, bgA = state.hexToRgb(self.props.background or "#000000")
    local fgR, fgG, fgB, fgA = state.hexToRgb(self.props.foreground or "#ffffff")
    local radius = self.props.borderRadius or 4
    local alpha = self.props.opacity or 0.5
    local pad = self.props.padding or 4

    System.drawRoundedRect(x, y, w, h, radius, {bgR, bgG, bgB, (bgA or 1.0) * alpha})

    local ty = y + pad
    if line1 then
        System.drawText(line1, x + pad, ty, 14, {fgR, fgG, fgB, fgA or 1.0})
        ty = ty + 16
    end
    if line2 then
        System.drawText(line2, x + pad, ty, 14, {fgR, fgG, fgB, (fgA or 1.0) * 0.7})
    end
end

local Spectrum = Widget.mkType("Spectrum", Widget)

function Spectrum:init(def)
  Widget.init(self, def)
  self.gl = GraticuleLegend.new("spec-legend", self)
  self.signalBoxWidgets = {}
  self.dragging = false
end

function Spectrum:onEvent(event)
  if event.type == "mouseWheel" then
    local step = 100
    if isCtrlDown and isCtrlDown() then step = 10000
    elseif isShiftDown and isShiftDown() then step = 100000 end
    local current = Model.rx.VFO.activeValue:get()
    Model.set("rx.VFO.activeValue", current + (event.delta or 0) * step)
    return true
  elseif event.type == "mouseButton" and event.button == "LEFT" then
    if event.isDown then
      self.dragging = true
      local freq = self:getFreqAtPx(event.x)
      Model.set("rx.VFO.activeValue", freq)
      state.setActive(self.id)
      if self.lwc then self.lwc:addTag("state.Active") end
      return true
    else
      self.dragging = false
      state.setActive(nil)
      if self.lwc then self.lwc:removeTag("state.Active") end
      return true
    end
  elseif event.type == "mouseMotion" and self.dragging then
    local freq = self:getFreqAtPx(event.x)
    Model.set("rx.VFO.activeValue", freq)
    return true
  end
  return Widget.onEvent(self, event)
end

function Spectrum:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 400 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 200 end
end

function Spectrum:getHzPerPx()
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = (_G.sampleRate or 384000) / zoom
  return span / math.max(1, self.props.w or 1)
end

function Spectrum:getFreqAtPx(px)
  local w = math.max(1, self.props.w or 1)
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = (_G.sampleRate or 384000) / zoom
  local center = Model.spectrumCenterFreq:peek()
  return center + (px / w - 0.5) * span
end

function Spectrum:getPxAtFreq(freq)
  local w = math.max(1, self.props.w or 1)
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = (_G.sampleRate or 384000) / zoom
  local center = Model.spectrumCenterFreq:peek()
  return (w / 2) + ((freq - center) / span) * w
end

function Spectrum:updateKids()
  local boxes = Model.signalBoxes:get() or {}
  local w, h = self.props.w or 0, self.props.h or 0
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = (_G.sampleRate or 384000) / zoom

  -- Ensure we have one widget per model box
  for i, box in ipairs(boxes) do
    if not self.signalBoxWidgets[i] then
      self.signalBoxWidgets[i] = SignalBox({ id = "sb-" .. tostring(box.id or i), index = i, parent = self })
      self:add(self.signalBoxWidgets[i])
    end

    local widget = self.signalBoxWidgets[i]
    widget.boxIndex = i
    local bw = box.bandwidth or 3000
    local freq = box.frequency or 0

    local boxW = (bw / span) * w
    local centerX = self:getPxAtFreq(freq)
    local boxX = centerX - (boxW / 2)

    widget.props.x = boxX
    widget.props.y = 0
    widget.props.w = boxW
    widget.props.h = h
  end

  -- Prune extra widgets if boxes were deleted
  for i = #self.signalBoxWidgets, #boxes + 1, -1 do
    local removed = table.remove(self.signalBoxWidgets, i)
    for k = #self.kids, 1, -1 do
      if self.kids[k] == removed then
        table.remove(self.kids, k)
        break
      end
    end
    if removed and removed.dispose then
      removed:dispose()
    end
  end
end

function Spectrum:draw()
  self:updateKids()
  return Widget.draw(self)
end

function Spectrum:drawSelf(spectrumData)
  local w, h = self.props.w or 0, self.props.h or 0

  -- Use provided data or fallback to global cached hardware data
  local data = (type(spectrumData) == "table" and spectrumData ~= self) and spectrumData or _G.lastSpectrumData

  -- Local 0,0
  Hardware.renderSpectrum(data or {}, 0, 0, w, h)

  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = (_G.sampleRate or 384000) / zoom

  self.gl:draw("spec-legend", 10, 10, 100, 45, self.lwc, string.format("%.1f kHz/div", span / 10000), "20 dB/div")
end

return Spectrum
