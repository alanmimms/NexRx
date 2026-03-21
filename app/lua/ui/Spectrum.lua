local Widget = require("ui.Widget")
local state = require("ui.State")
local Color = require("ui.Color")
local Model = require("Model")
local Hardware = require("Hardware")
local SignalBox = require("ui.SignalBox")
local events = require("Events")

--[[
  GraticuleLegend Widget
  Displays scaling info (e.g. "10 kHz/div") in corners of plot widgets.
]]
local GraticuleLegend = {}
GraticuleLegend.__index = GraticuleLegend

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

function GraticuleLegend.new()
    local self = setmetatable({}, GraticuleLegend)
    return self
end

function GraticuleLegend:draw(id, x, y, w, h, parentLWC, line1, line2)
    local lwc = setbox.newContext({"widget.GraticuleLegend", "id." .. id}, parentLWC)
    
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#000000"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local radius = lwc:optNumber("borderRadius", 4)
    local alpha = lwc:optNumber("opacity", 0.5)
    local pad = lwc:optNumber("padding", 4)
    
    System.drawRoundedRect(x, y, w, h, radius, {bgR, bgG, bgB, alpha})
    
    local ty = y + pad
    if line1 then
        System.drawText(line1, x + pad, ty, 14, {fgR, fgG, fgB, 1.0})
        ty = ty + 16
    end
    if line2 then
        System.drawText(line2, x + pad, ty, 14, {fgR, fgG, fgB, 0.7})
    end
end

local Spectrum = Widget.mkType("Spectrum", Widget)

function Spectrum:init(def)
  Widget.init(self, def)
  self.gl = GraticuleLegend.new()
  self.signalBoxWidgets = {}
end

function Spectrum:calcMetrics()
  if self.metrics.prefW == 0 then self.metrics.prefW = 400 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 200 end
end

function Spectrum:getHzPerPx()
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = _G.sampleRate / zoom
  return span / self.props.w
end

function Spectrum:getFreqAtPx(px)
  local w = self.props.w
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = _G.sampleRate / zoom
  local center = Model.spectrumCenterFreq:peek()
  return center + (px / w - 0.5) * span
end

function Spectrum:getPxAtFreq(freq)
  local w = self.props.w
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = _G.sampleRate / zoom
  local center = Model.spectrumCenterFreq:peek()
  return (w / 2) + ((freq - center) / span) * w
end

function Spectrum:updateKids()
  local boxes = Model.signalBoxes:get()
  local w, h = self.props.w, self.props.h
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = _G.sampleRate / zoom

  -- Ensure we have one widget per model box
  for i, box in ipairs(boxes) do
    if not self.signalBoxWidgets[i] then
      self.signalBoxWidgets[i] = SignalBox{ id = "sb-" .. box.id, index = i }
      self:add(self.signalBoxWidgets[i])
    end
    
    local widget = self.signalBoxWidgets[i]
    local bw = box.bandwidth
    local freq = box.frequency
    
    local boxW = (bw / span) * w
    local centerX = self:getPxAtFreq(freq)
    local boxX = centerX - (boxW / 2)
    
    widget.props.x = boxX
    widget.props.y = 0
    widget.props.w = boxW
    widget.props.h = h
  end
  
  -- Remove extra widgets if boxes were deleted (TBD)
end

function Spectrum:draw(parentLWC)
  self:updateKids()
  return Widget.draw(self, parentLWC)
end

function Spectrum:drawSelf(spectrumData)
  local w, h = self.props.w, self.props.h
  
  -- Use provided data or fallback to global cached hardware data
  local data = (type(spectrumData) == "table" and spectrumData ~= self) and spectrumData or _G.lastSpectrumData
  
  -- Local 0,0
  Hardware.renderSpectrum(data or {}, 0, 0, w, h)
  
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = _G.sampleRate / zoom
  
  self.gl:draw("spec-legend", 10, 10, 100, 45, self.lwc, string.format("%.1f kHz/div", span/10000), "20 dB/div")
end

return Spectrum
