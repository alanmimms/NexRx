local Widget = require("ui.Widget")
local Color = require("ui.Color")
local Stick = require("ui.Stick")
local Slider = require("ui.Slider")
local Spectrum = require("ui.Spectrum")
local Waterfall = require("ui.Waterfall")

-- Labels
local fpsLabel = Widget.Label{name = "fpsLabel", props = {text = "120 FPS"}, metrics = {stick = Stick.L}}
local ppsLabel = Widget.Label{name = "ppsLabel", props = {text = "12 PPS"}, metrics = {stick = Stick.R}}

-- Mode buttons (Discrete Slider)
local modeButtons = Slider.DiscreteSlider{
  name = "modeButtons", 
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
local volSlider = Slider{name = "volSlider", metrics = {stick = Stick.TLR, prefH = 40}}
local rfGainSlider = Slider{name = "rfGainSlider", metrics = {stick = Stick.TLR, prefH = 40}}

-- Sidebar
local lSidebar = Widget.Column{
  name = "lSidebar",
  borderColor = Color("#F00"),
  backgroundColor = Color("#300"),
  showBackground = true,
  showBorder = true,
  metrics = {stick = Stick.TLB, prefW = 200, flexW = 0},
  kids = {
    Widget.Row{name = "lSidebar.row", kids = {fpsLabel, ppsLabel}, metrics = {stick = Stick.TLR, prefH = 30}},
    Widget.Column{name = "lSidebar.column",kids = {modeButtons, volSlider, rfGainSlider}, metrics = {stick = Stick.TLBR, flexH = 1}}
  },
}

-- Center
local freqLabel = Widget.Label{name = "mhz", props = {text = "14.200.000 MHz", fontSize = 40}, metrics = {stick = Stick.TLBR, flexW = 1}}
local sMeter = Widget.Label{name = "s-meter", props = {text = "S9+20dB"}, metrics = {stick = Stick.TLBR, prefW = 150}}

local spectrum = Spectrum{name = "spectrum", metrics = {stick = Stick.TLBR, flexH = 1}}
local waterfall = Waterfall{name = "waterfall", metrics = {stick = Stick.TLBR, flexH = 1}}

local centerBar = Widget.Column{
  name = "centerBar",
  borderColor = Color("#0F0"),
  showBorder = true,
  metrics = {stick = Stick.TLBR, flexW = 1},
  kids = {
    Widget.Row{name = "centerBar.row", kids = {freqLabel, sMeter}, metrics = {stick = Stick.TLR, prefH = 75}},
    Widget.Column{name = "centerBar.column", kids = {spectrum, waterfall}, metrics = {stick = Stick.TLBR, flexH = 1}},
  },
}

-- Right Sidebar
local rSidebar = Widget.Column{
  name = "rSidebar",
  borderColor = Color("#880"),
  backgroundColor = Color("#220"),
  --  showBackground = true,
  showBorder = true,
  metrics = {stick = Stick.TLBR, prefW = 180, flexW = 0},
  kids = {
    Widget.Label{name = "'Right Sidebar'", props = {text = "Right Sidebar"}, borderColor = Color("#FFF"), showBorder = true, metrics = {stick = Stick.TLR}},
    Widget.Label{name = "'Settings'", props = {text = "Settings"}, borderColor = Color("#FFF"), showBorder = true, metrics = {stick = Stick.TLR}},
    Widget.Label{name = "'Tools'", props = {text = "Tools"}, borderColor = Color("#FFF"), showBorder = true, metrics = {stick = Stick.TLR}},
  },
}

-- Root Window
local rxTree = Widget.Window{
  name = "rxTree",
  kids = {
    Widget.Row{name = "rxTree.row",
      kids = {lSidebar, centerBar, rSidebar},
      metrics = {stick = Stick.TLBR, flexW = 1, flexH = 1}
    }
  }
}

local function renderUI(width, height)
  rxTree:layout(0, 0, width, height)
  rxTree:draw()
end

local function onResize(w, h)
  if rxTree and rxTree.onResize then
    rxTree:onResize(w, h)
  end
end

-- Event dispatchers called from C++
local function onMouseMove(x, y)
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  local focused = Widget.getFocused()
  if focused then
    focused:handleEvent({
	type = "mouseMotion",
	x = x, y = y
    })
  end
end

-- Revised dispatchers with full info
local function dispatchMouseEvent(type, x, y, button, isDown, mods)
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  if hit and type == "button" then
    hit:handleEvent({
	type = "mouseButton",
	button = button,
	isDown = isDown,
	x = x, y = y,
	mods = mods
    })
    -- Click to focus
    if isDown then
      hit:setFocus()
    end
  end
end

local function dispatchKeyEvent(key, isDown, mods)
  local focused = Widget.getFocused()
  if focused then
    focused:handleEvent({
	type = "key",
	key = key,
	isDown = isDown,
	mods = mods
    })
  end
end

return {
  render = renderUI,
  onResize = onResize,
  onMouseMove = onMouseMove,
  onMouseEvent = dispatchMouseEvent,
  onKeyEvent = dispatchKeyEvent
}
