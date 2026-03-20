local Widget = require("ui.Widget")
local state = require("ui.State")
local Color = require("ui.Color")
local Model = require("Model")
local Hardware = require("Hardware")
local GraticuleLegend = require("ui.GraticuleLegend")
local SignalBox = require("ui.SignalBox")
local events = require("Events")

local Spectrum = Widget.mkType("Spectrum", Widget)

function Spectrum:init(def)
  Widget.init(self, def)
  self.gl = GraticuleLegend.new()
  self.sb = SignalBox.new()
end

function Spectrum:calcMetrics()
  if self.metrics.prefW == 0 then self.metrics.prefW = 400 end
  if self.metrics.prefH == 0 then self.metrics.prefH = 200 end
end

function Spectrum:drawSelf(spectrumData)
  local w, h = self.props.w, self.props.h
  local id = self.id
  local parentLWC = self.lwc
  
  -- Use provided data or fallback to global cached hardware data
  local data = (type(spectrumData) == "table" and spectrumData ~= self) and spectrumData or _G.lastSpectrumData
  
  -- Local 0,0
  Hardware.renderSpectrum(data or {}, 0, 0, w, h)
  
  local zoom = Model.waterfall.zoom:get() or 1.0
  local span = _G.sampleRate / zoom
  local center = Model.spectrumCenterFreq:peek()
  
  -- Render SignalBoxes
  local boxes = Model.signalBoxes:get()
  local selectedIdx = Model.selectedSignalBoxIndex:get()
  
  for i, box in ipairs(boxes) do
     local bw = box.bandwidth
     local freq = box.frequency
     local boxW = (bw / span) * w
     local boxX = (w / 2) + ((freq - center) / span) * w - (boxW / 2)
     
     local isSelected = (i == selectedIdx)
     local tags = {"widget.SignalBox"}
     if isSelected then table.insert(tags, "state.Selected") end
     if box.ghost then table.insert(tags, "state.Ghost") end
     
     local label = box.name or tostring(box.id)
     if isSelected and events.hasModeTag("state.SbNamingMode") then
        label = _G.sbNamingText .. "|"
     end
     
     self.sb:draw("sb-" .. box.id, boxX, 0, boxW, h, parentLWC, label, tags, { index = i })
  end
  
  self.gl:draw("spec-legend", 10, 10, 100, 45, parentLWC, string.format("%.1f kHz/div", span/10000), "20 dB/div")
  state.registerWidget(self.id, {x=0, y=0, w=w, h=h}, self.tags)
end

return Spectrum
