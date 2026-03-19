--[[
   NexRx Application - Main Lua Entry Point
]]

io.stdout:setvbuf("no")
print("[Main] New Script loading...")

-- Core systems
require("SetBox")
local R = require("Reactive")

-- UI Framework
local Widget = require("ui.Widget")
local Color = require("ui.Color")
local Stick = require("ui.Stick")
local SliderNew = require("ui.Slider") 
local SpectrumNew = require("ui.Spectrum")
local WaterfallNew = require("ui.Waterfall")

-- Original App UI Modules
local uiState = require("ui.State")
local Hardware = require("Hardware")
local Model = require("Model")
local AppController = require("AppController")
local events = require("Events")
local animate = require("Animate")
local keys = require("Keycodes")
local smeterCalc = require("SMeterCalc")
_G.bands = require("Bands")
_G.calibration = require("Calibration")

-- Original Widget Classes
local Panel = require("ui.Panel")
local SMeterWidget = require("ui.SMeter")
local ActiveTags = require("ui.ActiveTags")
local GraticuleLegend = require("ui.GraticuleLegend")
local SignalBox = require("ui.SignalBox")
local ButtonOrig = require("ui.Button")
local SliderOrig = require("ui.SliderOld")
local FrequencyDisplayOrig = require("ui.FrequencyDisplay")
local Label = require("ui.Label")

_G.sampleRate = 96000
local spectrumData = {}
local frameCount = 0
local fps = 0
local rxTree = nil

-- Data for widgets
_G.freqEntryText = ""; _G.freqEntryCursor = 0

-- Export UI module for C++ hooks (MUST BE DECLARED BEFORE init() IS CALLED)
local function renderUI(width, height)
  if not rxTree then return end
  rxTree:layout(0, 0, width, height)
  
  uiState.beginFrame() -- Needed for original widgets
  rxTree:draw()
  uiState.endFrame()
end

local function onResize(w, h)
  if rxTree then rxTree:onResize(w, h) end
end

local function onMouseMove(x, y)
  Widget.updateGlobalMouse(rxTree, x, y)
end

local function onMouseEvent(type, x, y, button, isDown, mods)
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  if hit and type == "button" then
    hit:handleEvent({
	type = "mouseButton",
	button = button,
	isDown = isDown,
	x = x, y = y,
	mods = mods
    })
    if isDown then hit:setFocus() end
  end
end

local function onKeyEvent(key, isDown, mods)
  local focused = Widget.getFocused()
  if focused and focused.handleEvent then
    focused:handleEvent({type = "key", key = key, isDown = isDown, mods = mods})
  end
end

_G.UI = {
  render = renderUI,
  onResize = onResize,
  onMouseMove = onMouseMove,
  onMouseEvent = onMouseEvent,
  onKeyEvent = onKeyEvent
}

function init()
   print("[Main] init() starting...")
   
   -- Load configurations
   local configFiles = {
      "config/default.lua",
      "config/settings.lua",
      "config/bands.lua",
      "config/colormaps.lua",
      "config/events.lua",
      "config/constraints.lua"
   }
   for _, file in ipairs(configFiles) do setbox.loadFile(file) end

   AppController.init()
   _G.calibration.init()
   _G.bands.init()
   events.init()
   
   -- Hardware
   if hw.connect("127.0.0.1", 5000, 5001) then 
      Hardware.enableHardware() 
      Hardware.enableWaterfall()
   end
   
   if audio.isInitialized() then audio.start() end
   waterfall.init(1024, 256)
   waterfall.setRange(Model.waterfall.minDB:get(), Model.waterfall.maxDB:get())
   if _G.colormaps and _G.colormaps[Model.waterfall.colormap:get()] then 
      waterfall.setColormapData(_G.colormaps[Model.waterfall.colormap:get()]) 
   end

   -- Instantiate Original Widgets for bridging
   local wSMeter = SMeterWidget.new()
   local wFreq = FrequencyDisplayOrig.new({ valueObs = Model.rx.VFO.activeValue })
   local wSlider = SliderOrig.new({ valueObs = Model.rx.VFO.activeValue })
   local wVol = SliderOrig.new({ valueObs = Model.rx.volume.DB })
   local wGain = SliderOrig.new({ valueObs = Model.rx.RF.gainDB })
   local wTags = ActiveTags.new()

   -- Sidebar
   local leftSidebar = Widget.Column{
      name = "left-sidebar", tags = {"widget.Sidebar", "widget.LeftSidebar"},
      metrics = { stick = Stick.TLB, prefW = 280, margin = {left=8, right=8, top=8, bottom=8} },
      kids = {
         Widget.BridgeWidget{ id = "id-rx-freq", orig = wFreq, metrics = { stick = Stick.TLR, prefH = 40 },
            drawArgs = { _G.freqEntryText, _G.freqEntryCursor, {"VFOControl"} }
         },
         Widget.BridgeWidget{ id = "id-rx-slider", orig = wSlider, metrics = { stick = Stick.TLR, prefH = 20 },
            drawArgs = { 0.1e6, 30.0e6, 14.2e6 } 
         },
         Widget.Label{text = "Mode", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.Row{
            metrics = {stick = Stick.TLR, prefH = 30},
            kids = (function()
               local ks = {}
               for _, m in ipairs({"LSB", "USB", "AM", "CW", "FM"}) do
                  table.insert(ks, Widget.Button{
                     text = m, metrics = {flexW = 1},
                     onClicked = function() Model.set("rx.selectedMode", m) end
                  })
               end
               return ks
            end)()
         },
         Widget.Label{text = "Band", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.Row{
            metrics = {stick = Stick.TLR, prefH = 30},
            kids = (function()
               local ks = {}
               for _, b in ipairs({"160m","80m","40m","20m","15m","10m"}) do
                  table.insert(ks, Widget.Button{
                     text = b, metrics = {flexW = 1},
                     onClicked = function() Model.set("rx.selectedBand", b) end
                  })
               end
               return ks
            end)()
         },
         Widget.Label{text = "Volume", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.BridgeWidget{ id = "id-rx-vol", orig = wVol, metrics = { stick = Stick.TLR, prefH = 20 },
            drawArgs = { -60, 0, 0 }
         },
         Widget.Label{text = "RF Gain", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.BridgeWidget{ id = "id-rx-gain", orig = wGain, metrics = { stick = Stick.TLR, prefH = 20 },
            drawArgs = { -20, 60, 0 }
         }
      }
   }

   local centerArea = Widget.Column{
      name = "center-area", tags = {"widget.CenterArea"},
      metrics = { stick = Stick.TLBR, flexW = 1 },
      kids = {
         Widget.BridgeWidget{ 
            id = "id-spec", tags = {"widget.Spectrum", "widget.VFOControl"},
            metrics = { stick = Stick.TLBR, flexH = 1, minH = 150 },
            orig = {
               id = "id-spec", tags = {"widget.Spectrum", "widget.VFOControl"},
               gl = GraticuleLegend.new(), sb = SignalBox.new(),
               draw = function(self, id, x, y, w, h, parentLWC)
                  Hardware.renderSpectrum(spectrumData, x, y, w, h)
                  local span = _G.sampleRate / (Model.waterfall.zoom:get() or 1.0)
                  self.gl:draw("spec-legend", x + 10, y + 10, 100, 45, parentLWC, string.format("%.1f kHz/div", span/10000), "20 dB/div")
                  events.registerWidget(self.id, {x=x, y=y, w=w, h=h}, self.tags)
               end
            }
         },
         Widget.BridgeWidget{
            id = "id-wf", tags = {"widget.Waterfall", "widget.VFOControl"},
            metrics = { stick = Stick.TLBR, flexH = 1, minH = 200 },
            orig = {
               id = "id-wf", tags = {"widget.Waterfall", "widget.VFOControl"},
               draw = function(self, id, x, y, w, h, parentLWC)
                  Hardware.renderWaterfall(x, y, w, h, Model.waterfall.zoom:get(), 0.5)
                  events.registerWidget(self.id, {x=x, y=y, w=w, h=h}, self.tags)
               end
            }
         }
      }
   }

   local activeTagsSidebar = Widget.Column{
      name = "active-tags-sidebar", tags = {"widget.Sidebar", "widget.ActiveTagsSidebar"},
      metrics = { stick = Stick.TRB, prefW = 180, margin = {left=8, right=8, top=8, bottom=8} },
      kids = {
         Widget.BridgeWidget{ id = "id-smeter", orig = wSMeter, metrics = { stick = Stick.TLR, prefH = 66 }, 
            onEvent = function(self, ev) return false end,
            drawArgs = {} 
         },
         Widget.BridgeWidget{ id = "id-active-tags", orig = wTags, metrics = { flexH = 1 } }
      }
   }

   rxTree = Widget.Window{
      name = "id-root-window",
      kids = {
         Widget.Column{
            name = "rootColumn",
            metrics = { stick = Stick.TLBR, flexW = 1, flexH = 1 },
            kids = {
               Widget.Row{
                  name = "top-bar", metrics = { stick = Stick.TLR, prefH = 32 },
                  backgroundColor = Color("#111b2b"),
                  kids = {
                     Widget.Label{ name = "titleLabel", text = "NexRx SDR", metrics = {stick = Stick.L, flexW = 1} },
                     Widget.Label{ name = "fpsLabel", text = "0 FPS", metrics = {stick = Stick.R, prefW = 100} }
                  }
               },
               Widget.Row{
                  name = "main-area", metrics = { stick = Stick.TLBR, flexW = 1, flexH = 1 },
                  kids = { leftSidebar, centerArea, activeTagsSidebar }
               }
            }
         }
      }
   }

   print("[Main] init() complete.")
end

function update(dt)
   _G.frameCount = (_G.frameCount or 0) + 1
   animate.update(dt)
   
   -- FPS calculation
   fps = 1.0 / dt
   local fpsLabel = rxTree:findByName("fpsLabel")
   if fpsLabel then
      fpsLabel.text = string.format("%.0f FPS", fps)
   end

   -- Input
   uiState.beginFrame()

   AppController.sync()
   
   if Hardware.isHardwareEnabled() then
      if _G.frameCount % 10 == 0 then AppController.pollState() end
      local hwSpec = Hardware.getSpectrum()
      if hwSpec and #hwSpec > 0 then 
         spectrumData = hwSpec
         Hardware.updateWaterfall(spectrumData) 
      end
   else
      -- Dummy data for UI testing
      if _G.frameCount % 2 == 0 then
         local dummy = {}
         for i = 1, 1024 do dummy[i] = -100 + 20 * math.sin(i / 50 + _G.frameCount/10) + 5 * math.random() end
         spectrumData = dummy
         Hardware.updateWaterfall(spectrumData)
      end
   end

   -- Update BridgeWidget drawArgs for dynamic components
   local smW = rxTree:findByName("id-smeter")
   if smW then smW.drawArgs = { smeterCalc.getReading() } end

   local freqW = rxTree:findByName("id-rx-freq")
   if freqW then freqW.drawArgs = { _G.freqEntryText, _G.freqEntryCursor, {"VFOControl"} } end

   local sliderW = rxTree:findByName("id-rx-slider")
   if sliderW then sliderW.drawArgs = { 0.1e6, 30.0e6, Model.rx.VFO.activeValue:peek() } end

   local volW = rxTree:findByName("id-rx-vol")
   if volW then volW.drawArgs = { -60, 0, Model.rx.volume.DB:peek() } end

   local gainW = rxTree:findByName("id-rx-gain")
   if gainW then gainW.drawArgs = { -20, 60, Model.rx.RF.gainDB:peek() } end
end
