local Widget = require("Widget")
local Color = require("Color")
local Stick = require("Stick")
local Slider = require("Slider")
local Spectrum = require("Spectrum")
local Waterfall = require("Waterfall")

-- Labels
local fpsLabel = Widget.Label{props = {text = "120 FPS"}, metrics = {stick = Stick.L}}
local ppsLabel = Widget.Label{props = {text = "12 PPS"}, metrics = {stick = Stick.R}}

-- Mode buttons (Discrete Slider)
local modeButtons = Slider.DiscreteSlider{
  labelPos = "above",
  stops = {
    {label = "USB", value = "USB"},
    {label = "LSB", value = "LSB"},
    {label = "CW",  value = "CW"},
    {label = "AM",  value = "AM"}
  },
  metrics = {stick = Stick.TLR}
}

-- Sliders
local volSlider = Slider{metrics = {stick = Stick.TLR, prefH = 40}}
local rfGainSlider = Slider{metrics = {stick = Stick.TLR, prefH = 40}}

-- Sidebar
local lSidebar = Widget.Column{
   borderColor = Color("#F00"),
   backgroundColor = Color("#200"),
   showBackground = true,
   showBorder = true,
   metrics = {stick = Stick.TLB, prefW = 200},
   kids = {
     Widget.Row{kids = {fpsLabel, ppsLabel}, metrics = {stick = Stick.TLR, prefH = 30}},
     Widget.Column{kids = {modeButtons, volSlider, rfGainSlider}, metrics = {stick = Stick.TLBR, flexH = 1}}
   },
}

-- Center
local freqLabel = Widget.Label{props = {text = "14.200.000 MHz", fontSize = 40}, metrics = {stick = Stick.TLBR, flexW = 1}}
local sMeter = Widget.Label{props = {text = "S9+20dB"}, metrics = {stick = Stick.TLBR, prefW = 150}}

local spectrum = Spectrum{metrics = {stick = Stick.TLBR, flexH = 1}}
local waterfall = Waterfall{metrics = {stick = Stick.TLBR, flexH = 1}}

local centerBar = Widget.Column{
   borderColor = Color("#0F0"),
   showBorder = true,
   metrics = {stick = Stick.TLBR, flexW = 1},
   kids = {
     Widget.Row{kids = {freqLabel, sMeter}, metrics = {stick = Stick.TLR, prefH = 100}},
     Widget.Column{kids = {spectrum, waterfall}, metrics = {stick = Stick.TLBR, flexH = 1}},
   },
}

-- Right Sidebar
local rSidebar = Widget.Column{
  borderColor = Color("#00F"),
  backgroundColor = Color("#002"),
  showBackground = true,
  showBorder = true,
  metrics = {stick = Stick.TRB, prefW = 180},
  kids = {
    Widget.Column{metrics = {stick = Stick.TLBR, flexH = 1},
      kids = {
	Widget.Label{props = {text = "Right Sidebar"}, metrics = {stick = Stick.TLR}},
	Widget.Label{props = {text = "Settings"}, metrics = {stick = Stick.TLR}},
	Widget.Label{props = {text = "Tools"}, metrics = {stick = Stick.TLR}},
    }},
  },
}

-- Root Window
-- Window is an hFlow container by default, so we can put sidebars directly in it.
local rxTree = Widget.Window{
  kids = {lSidebar, centerBar, rSidebar}
}

local function renderUI(bridge, width, height)
  rxTree:layout(bridge, 0, 0, width, height)
  rxTree:draw(bridge)
end

local function onResize(w, h)
  if rxTree and rxTree.onResize then
    rxTree:onResize(w, h)
  end
end

return {
  render = renderUI,
  onResize = onResize
}
