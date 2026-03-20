--[[
   NexRx Application - Main Lua Entry Point
]]

io.stdout:setvbuf("no")
print("[Main] Script loading...")

-- Core systems
require("SetBox")
local R = require("Reactive")

-- UI Framework
local Widget = require("ui.Widget")
local Color = require("ui.Color")
local Stick = require("ui.Stick")
local Layout = require("ui.Layout")

-- Original App UI Modules
local uiState = require("ui.State")
local Hardware = require("Hardware")
local Model = require("Model")
local AppController = require("AppController")
local events = require("Events")
local animate = require("Animate")
local keys = require("Keycodes")
local Modes = require("Modes")
_G.bands = require("Bands")
_G.calibration = require("Calibration")

-- Widget Classes
local Panel = require("ui.Panel")
local SMeter = require("ui.SMeter")
local ActiveTags = require("ui.ActiveTags")
local GraticuleLegend = require("ui.GraticuleLegend")
local SignalBox = require("ui.SignalBox")
local Button = require("ui.Button")
local Slider = require("ui.Slider")
local FrequencyDisplay = require("ui.FrequencyDisplay")
local Label = require("ui.Label")
local Waterfall = require("ui.Waterfall")
local Spectrum = require("ui.Spectrum")

_G.lowestFreq = 100.0e3
_G.highestFreq = 30.0e6
_G.sampleRate = 96.0e3
local spectrumData = {}
local frameCount = 0
local fps = 0
local rxTree = nil

-- Data for widgets
_G.freqEntryText = ""; _G.freqEntryCursor = 0

print("[Main] Script loaded.")

-- Export UI module for C++ hooks (MUST BE DECLARED BEFORE init() IS CALLED)
local function renderUI(width, height)
  if not rxTree then return end
  rxTree:layout(0, 0, width, height)

  _G.drawOffsetX = 0
  _G.drawOffsetY = 0
  
  -- Clear background
  System.drawRect(0, 0, width, height, {0.05, 0.05, 0.1, 1.0})

  uiState.beginFrame(frameInput)
  rxTree:draw()
  uiState.endFrame()

  -- Clear one-shot click/release for next frame
  frameInput.mouseClicked = false
  frameInput.mouseReleased = false
  frameInput.mouseWheel = 0
end
local function onResize(w, h)
  if rxTree then rxTree:onResize(w, h) end
end

local function onMouseMove(x, y)
  -- print("[Main] MouseMove", x, y)
  frameInput.mouseX, frameInput.mouseY = x, y
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  
  local handled = false
  if hit and hit.handleEvent then
    handled = hit:handleEvent({
      type = "mouseMotion",
      x = x, y = y
    })
  end
  
  if not handled then
    events.dispatch(events.createEvent(events.Type.MOUSE_MOVE, { x=x, y=y }))
  end
end

local function translateMods(mods)
  local t = {}
  if mods then
    if (mods & 1) ~= 0 then table.insert(t, "input.SHIFT") end
    if (mods & 2) ~= 0 then table.insert(t, "input.CTRL") end
    if (mods & 4) ~= 0 then table.insert(t, "input.ALT") end
  end
  return t
end

local function onMouseEvent(type, x, y, button, isDown, mods)
  -- print("[Main] MouseEvent", type, x, y, button, isDown)
  frameInput.mouseX, frameInput.mouseY = x, y
  if type == "button" then
    frameInput.mouseDown = isDown
    if isDown then 
        frameInput.mouseClicked = true 
    else 
        frameInput.mouseReleased = true 
    end
  elseif type == "wheel" then
    frameInput.mouseWheel = button -- delta
  end

  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  -- if type == "button" and isDown then print("[Main] Hit widget:", hit and hit.name or "NIL") end
  
  local eventData = {
    type = (type == "wheel") and "mouseWheel" or "mouseButton",
    button = button == 0 and "LEFT" or (button == 1 and "MIDDLE" or "RIGHT"),
    isDown = isDown,
    x = x, y = y,
    delta = type == "wheel" and button or 0,
    modifiers = translateMods(mods)
  }

  -- Dispatch to Widget hierarchy (using GLOBAL coordinates)
  local handled = false
  if hit and hit.handleEvent then
    handled = hit:handleEvent(eventData)
    if isDown then hit:setFocus() end
  end

  -- If not handled by widget tree, try global rules
  if not handled then
    local et = (type == "wheel") and events.Type.MOUSE_WHEEL or 
               (isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP)
    events.dispatch(events.createEvent(et, eventData))
  end
end

local function onKeyEvent(key, isDown, mods)
  local focused = Widget.getFocused()
  local handled = false
  local eventData = {
    type = "key",
    key = keys.getName(key) or tostring(key),
    isDown = isDown,
    modifiers = translateMods(mods)
  }

  if focused and focused.handleEvent then
    handled = focused:handleEvent(eventData)
  end
  
  if not handled then
    local et = isDown and events.Type.KEY_DOWN or events.Type.KEY_UP
    events.dispatch(events.createEvent(et, eventData))
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
      "config/events.lua"
   }
   for _, file in ipairs(configFiles) do setbox.loadFile(file) end

   AppController.init()
   _G.calibration.init()
   _G.bands.init()
   events.init()
   uiState.setEventsModule(events)
   
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

   local modeStops = {}
   for _, name in ipairs(Modes.names) do
      table.insert(modeStops, {label = name, value = name})
   end

   local bandStops = {}
   for _, name in ipairs(_G.bands.getAllNames()) do
      table.insert(bandStops, {label = name, value = name})
   end

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
                     Widget.Label{ id = "titleLabel", text = "NexRx SDR", metrics = {stick = Stick.L, flexW = 1} },
                     Widget.Label{ id = "fpsLabel", text = "0 FPS", metrics = {stick = Stick.R, prefW = 100} }
                  }
               },
               Widget.Row{
                  name = "main-area", metrics = { stick = Stick.TLBR, flexW = 1, flexH = 1 },
                  kids = {
                     Widget.Column{
                        name = "left-sidebar", tags = {"widget.Sidebar", "widget.LeftSidebar"},
                        metrics = { stick = "TLB", prefW = 280, margin = 8 },
                        kids = {
                           FrequencyDisplay{ id = "id-rx-freq", valueObs = Model.rx.VFO.activeValue, tags = {"widget.VFOControl"}, metrics = { stick = "TLR", prefH = 40 } },
                           Slider{ id = "id-rx-slider", valueObs = Model.rx.VFO.activeValue, minVal = _G.lowestFreq, maxVal = _G.highestFreq, metrics = { stick = "TLR", prefH = 20 } },
                           Widget.Label{text = "Mode", metrics = {stick = "TLR", prefH = 20, margin={top=10}}},
                           Slider.DiscreteSlider{ id = "id-mode-slider", valueObs = Model.rx.selectedMode, stops = modeStops, metrics = { stick = "TLR", prefH = 30 } },
                           Widget.Label{text = "Band", metrics = {stick = "TLR", prefH = 20, margin={top=10}}},
                           Slider.DiscreteSlider{ id = "id-band-slider", valueObs = Model.rx.selectedBand, stops = bandStops, metrics = { stick = "TLR", prefH = 30 } },
                           Widget.Label{text = "Volume", metrics = {stick = "TLR", prefH = 20, margin={top=10}}},
                           Slider{ id = "id-rx-vol", valueObs = Model.rx.volume.DB, minVal = -60, maxVal = 0, metrics = { stick = "TLR", prefH = 20 } },
                           Widget.Label{text = "RF Gain", metrics = {stick = "TLR", prefH = 20, margin={top=10}}},
                           Slider{ id = "id-rx-gain", valueObs = Model.rx.RF.gainDB, minVal = -20, maxVal = 60, metrics = { stick = "TLR", prefH = 20 } }
                        }
                     },
                     Widget.Column{
                        name = "center-area", tags = {"widget.CenterArea"},
                        metrics = { stick = "TLBR", flexW = 1 },
                        kids = {
                           Spectrum{ id = "id-spec", tags = {"widget.Spectrum", "widget.VFOControl"}, metrics = { stick = "TLBR", flexH = 1, minH = 150 } },
                           Waterfall{ id = "id-wf", tags = {"widget.Waterfall", "widget.VFOControl"}, metrics = { stick = "TLBR", flexH = 1, minH = 200 } }
                        }
                     },
                     Widget.Column{
                        name = "active-tags-sidebar", tags = {"widget.Sidebar", "widget.ActiveTagsSidebar"},
                        metrics = { stick = "TRB", prefW = 180, margin = 8 },
                        kids = {
                           SMeter{ id = "id-smeter", metrics = { stick = "TLR", prefH = 66 } },
                           ActiveTags{ id = "id-active-tags", metrics = { flexH = 1 } }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   print("[Main] init() complete.")
end

local frameInput = {
   mouseX = 0, mouseY = 0,
   mouseDown = false, mouseClicked = false, mouseReleased = false,
   mouseWheel = 0
}

function update(dt)
   _G.frameCount = (_G.frameCount or 0) + 1
   animate.update(dt)
   events.clearWidgets()
   
   -- FPS calculation
   fps = 1.0 / dt
   local fpsLabel = rxTree:findByName("fpsLabel")
   if fpsLabel then
      fpsLabel.props.text = string.format("%.0f FPS", fps)
   end

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
end
