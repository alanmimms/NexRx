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
local SignalBox = require("ui.SignalBox")
local Button = require("ui.Button")
local Slider = require("ui.Slider")
local FrequencyDisplay = require("ui.FrequencyDisplay")
local Label = require("ui.Label")
local Waterfall = require("ui.Waterfall")
local Spectrum = require("ui.Spectrum")
local RiverView = require("ui.RiverView")
local FrequencyRuler = require("ui.FrequencyRuler")
local LiveTrace = require("ui.LiveTrace")
local GutterLeft = require("ui.GutterLeft")

_G.lowestFreq = 100.0e3
_G.highestFreq = 30.0e6
_G.sampleRate = 384.0e3
_G.lastSpectrumData = {}
local frameCount = 0
local fps = 0
local rxTree = nil

-- Data for widgets
_G.freqEntryText = ""; _G.freqEntryCursor = 0

local frameInput = {
   mouseX = 0, mouseY = 0,
   mouseDown = false, mouseClicked = false, mouseReleased = false,
   mouseWheel = 0
}

local function safeCall(fn, ...)
    local ok, err = xpcall(fn, debug.traceback, ...)
    if not ok then
        print("\n[LUA ERROR] " .. tostring(err) .. "\n")
    end
    return ok, err
end

print("[Main] Script loaded.")

-- Export UI module for C++ hooks
local function renderUI_inner(width, height)
  if not rxTree then return end
  rxTree:layout(0, 0, width, height)

  System.drawRect(0, 0, width, height, {0.05, 0.05, 0.1, 1.0})

  uiState.beginFrame(frameInput)
  rxTree:draw()
  uiState.endFrame()

  frameInput.mouseClicked = false
  frameInput.mouseReleased = false
  frameInput.mouseWheel = 0
end

local function renderUI(w, h) safeCall(renderUI_inner, w, h) end

local function onResize(w, h)
  if rxTree then safeCall(rxTree.onResize, rxTree, w, h) end
end

local currentMods = 0
_G.isShiftDown = function() return (currentMods & 1) ~= 0 end
_G.isCtrlDown = function() return (currentMods & 2) ~= 0 end
_G.isAltDown = function() return (currentMods & 4) ~= 0 end

local function translateMods(mods)
  local t = {}
  if mods then
    if (mods & 1) ~= 0 then table.insert(t, "input.SHIFT") end
    if (mods & 2) ~= 0 then table.insert(t, "input.CTRL") end
    if (mods & 4) ~= 0 then table.insert(t, "input.ALT") end
  end
  return t
end

local function onMouseMove_inner(x, y)
  frameInput.mouseX, frameInput.mouseY = x, y
  if not rxTree then return end

  local eventData = {
    type = "mouseMotion",
    x = x, y = y,
    modifiers = translateMods(currentMods)
  }

  local activeId = uiState.getActive()
  local activeWidget = activeId and rxTree:findByID(activeId)
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  local target = activeWidget or hit or rxTree
  target:handleEvent(eventData)
end

local function onMouseMove(x, y) safeCall(onMouseMove_inner, x, y) end

local function onMouseEvent_inner(type, x, y, button, isDown, mods)
  currentMods = mods or currentMods
  frameInput.mouseX, frameInput.mouseY = x, y
  if type == "button" then
    frameInput.mouseDown = isDown
    if isDown then 
        frameInput.mouseClicked = true 
    else 
        frameInput.mouseReleased = true 
    end
  elseif type == "wheel" then
    frameInput.mouseWheel = button
  end

  if not rxTree then return end
  
  local eventData = {
    type = (type == "wheel") and "mouseWheel" or (type == "motion" and "mouseMotion" or "mouseButton"),
    isDown = isDown,
    x = x, y = y,
    delta = (type == "wheel") and button or 0,
    modifiers = translateMods(mods)
  }
  
  if type == "button" then
    eventData.button = button == 0 and "LEFT" or (button == 1 and "MIDDLE" or "RIGHT")
  end

  local activeId = uiState.getActive()
  local activeWidget = activeId and rxTree:findByID(activeId)
  
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  local target = (type ~= "wheel" and activeWidget) or hit or rxTree
  
  local handled = target:handleEvent(eventData)
  if handled and isDown and type == "button" then 
    target:setFocus() 
  end

  if type == "button" and not isDown then
    uiState.setActive(nil)
  end

  if not handled then
    handled = rxTree:handleEvent(eventData)
  end
  
  if not handled then
    events.dispatch(eventData, hit or rxTree)
  end
end

local function onMouseEvent(type, x, y, button, isDown, mods)
    safeCall(onMouseEvent_inner, type, x, y, button, isDown, mods)
end

local function onTextInput_inner(text)
  if not rxTree then return end
  local eventData = {
    type = "textInput",
    text = text
  }

  local target = Widget.getFocused() or Widget.getHovered() or rxTree
  local handled = false
  if target then
    handled = target:handleEvent(eventData)
  end
  
  if not handled and target ~= rxTree then
    handled = rxTree:handleEvent(eventData)
  end
  
  if not handled then
    events.dispatch(eventData, target)
  end
end

local function onTextInput(text) safeCall(onTextInput_inner, text) end

local function onKeyEvent_inner(key, isDown, mods)
  currentMods = mods or currentMods
  if not rxTree then return end
  local keyName = keys.getName(key) or tostring(key)

  local eventData = {
    type = "key",
    key = keyName,
    isDown = isDown,
    modifiers = translateMods(mods)
  }

  local target = Widget.getFocused() or Widget.getHovered() or rxTree
  local handled = false
  if target then
    handled = target:handleEvent(eventData)
  end
  
  if not handled and target ~= rxTree then
    handled = rxTree:handleEvent(eventData)
  end

  if not handled then
    events.dispatch(eventData, target)
  end
end

local function onKeyEvent(key, isDown, mods) safeCall(onKeyEvent_inner, key, isDown, mods) end

_G.UI = {
  render = renderUI,
  onResize = onResize,
  onMouseMove = onMouseMove,
  onMouseEvent = onMouseEvent,
  onKeyEvent = onKeyEvent,
  onTextInput = onTextInput
}

function init()
   print("[Main] init() starting...")
   
   uiState.setEventsModule(events)
   
   local configFiles = {
      "config/default.lua",
      "config/settings.lua",
      "config/bands.lua",
      "config/colormaps.lua",
      "config/events.lua"
   }
   for _, file in ipairs(configFiles) do 
      setbox.loadFile(file) 
   end

   AppController.init()
   _G.calibration.init()
   _G.bands.init()
   events.init()
   
   if hw.connect("127.0.0.1", 5000, 5001) then 
      Hardware.enableHardware() 
      Hardware.enableWaterfall()
   end
   
   if audio.isInitialized() then audio.start() end
   local riverEngine = _G.river or _G.waterfall
   if riverEngine and riverEngine.init then
      riverEngine.init(1024, 512)
      riverEngine.setRange(Model.waterfall.minDB:get(), Model.waterfall.maxDB:get())
      if _G.colormaps and _G.colormaps[Model.waterfall.colormap:get()] then 
         riverEngine.setColormapData(_G.colormaps[Model.waterfall.colormap:get()]) 
      end
   end

   local modeStops = {}
   for _, name in ipairs(Modes.names) do
      table.insert(modeStops, {label = name, value = name})
   end

   local bandStops = {}
   for _, name in ipairs(_G.bands.getAllNames()) do
      table.insert(bandStops, {label = name, value = name})
   end

   local initialVFO = Model.rx.VFO.activeValue:get() or 14.240e6
   local initialBand = _G.bands.getBand(Model.rx.selectedBand:get() or "20m")
   local initFMin = initialBand and initialBand.lowerEdge or 14.0e6
   local initFMax = initialBand and initialBand.upperEdge or 14.35e6

   local vfoDisp = FrequencyDisplay{
      id = "id-rx-freq",
      valueObs = Model.rx.VFO.activeValue,
      tags = {"widget.VFOControl"},
      metrics = { stick = Stick.L, prefW = 180, prefH = 32, margin = { left = 10, top = 3 } }
   }

   local riverView = RiverView{
      id = "id-river-view",
      tags = {"widget.RiverView"},
      timeSpan = 30.0,
      timeOffset = 0.0,
      freqMin = initFMin,
      freqMax = initFMax,
      centerFreq = initialVFO,
      bandwidth = 350e3,
      metrics = { stick = Stick.TLBR, flexW = 1, flexH = 1 },
      signalBoxes = {
         {
            id = "id-primary-box",
            index = 1,
            mode = Model.rx.selectedMode:get() or "USB",
            carrierFreq = initialVFO,
            bandwidth = 2800,
            label = "CQ W1AW"
         }
      }
   }

   local freqRuler = FrequencyRuler{
      id = "id-freq-ruler",
      tags = {"widget.FrequencyRuler"},
      freqMin = initFMin,
      freqMax = initFMax,
      metrics = { stick = Stick.TRB, prefW = 60, flexH = 1 },
      onFrequencyChange = function(self, fMin, fMax)
         riverView.freqMin = fMin
         riverView.freqMax = fMax
      end
   }

   local liveTrace = LiveTrace{
      id = "id-live-trace",
      tags = {"widget.LiveTrace"},
      freqMinNorm = 0.0,
      freqMaxNorm = 1.0,
      metrics = { stick = Stick.TRB, prefW = 90, flexH = 1 }
   }

   local gutterLeft = GutterLeft{
      id = "id-gutter-left",
      tags = {"widget.GutterLeft"},
      freqMin = initFMin,
      freqMax = initFMax,
      boxes = {
         { carrierFreq = initialVFO, mode = "USB" }
      },
      metrics = { stick = Stick.TLB, prefW = 68, flexH = 1 }
   }

   -- Reactive synchronization between VFO, Mode, Band, and The River
   Model.rx.VFO.activeValue:watch(function(freq)
      if riverView and freq then
         riverView.hwCenterFreq = freq
         if riverView.signalBoxes and riverView.signalBoxes[1] then
            riverView.signalBoxes[1].carrierFreq = freq
         end
         if gutterLeft and gutterLeft.activeBoxes and gutterLeft.activeBoxes[1] then
            gutterLeft.activeBoxes[1].carrierFreq = freq
         end
      end
   end)

   Model.rx.selectedMode:watch(function(mode)
      if riverView and riverView.signalBoxes and riverView.signalBoxes[1] and mode then
         riverView.signalBoxes[1].mode = mode
      end
   end)

   Model.rx.selectedBand:watch(function(bandName)
      if _G.bands and bandName then
         local b = _G.bands.getBand(bandName)
         if b and riverView and freqRuler and gutterLeft then
            riverView.freqMin = b.lowerEdge
            riverView.freqMax = b.upperEdge
            freqRuler.props.freqMin = b.lowerEdge
            freqRuler.props.freqMax = b.upperEdge
            gutterLeft.props.freqMin = b.lowerEdge
            gutterLeft.props.freqMax = b.upperEdge
         end
      end
   end)

   rxTree = Widget.Window{
      name = "id-root-window",
      kids = {
         Widget.Column{
            name = "rootColumn",
            metrics = { stick = Stick.TLBR, flexW = 1, flexH = 1 },
            kids = {
               Widget.Row{
                  name = "top-bar", metrics = { stick = Stick.TLR, prefH = 38 },
                  backgroundColor = Color("#0b1220"),
                  kids = {
                     Widget.Label{ id = "titleLabel", text = "NexRx SDR | The River", metrics = {stick = Stick.L, prefW = 160, margin = {left = 8, top = 8}} },
                     vfoDisp,
                     Widget.Label{ text = "Mode:", metrics = {stick = Stick.L, prefW = 42, margin = {left = 10, top = 8}} },
                     Slider.DiscreteSlider{ id = "id-mode-slider", valueObs = Model.rx.selectedMode, stops = modeStops, metrics = { stick = Stick.L, prefW = 130, prefH = 26, margin = {top = 6}} },
                     Widget.Label{ text = "Band:", metrics = {stick = Stick.L, prefW = 40, margin = {left = 10, top = 8}} },
                     Slider.DiscreteSlider{ id = "id-band-slider", valueObs = Model.rx.selectedBand, stops = bandStops, metrics = { stick = Stick.L, prefW = 110, prefH = 26, margin = {top = 6}} },
                     Widget.Label{ text = "Vol:", metrics = {stick = Stick.L, prefW = 28, margin = {left = 10, top = 8}} },
                     Slider{ id = "id-rx-vol", valueObs = Model.rx.volume.DB, minVal = -60, maxVal = 0, metrics = { stick = Stick.L, prefW = 75, prefH = 20, margin = {top = 9}} },
                     Widget.Label{ text = "Gain:", metrics = {stick = Stick.L, prefW = 34, margin = {left = 8, top = 8}} },
                     Slider{ id = "id-rx-gain", valueObs = Model.rx.RF.gainDB, minVal = -20, maxVal = 60, metrics = { stick = Stick.L, prefW = 75, prefH = 20, margin = {top = 9}} },
                     Widget.Label{ id = "fpsLabel", text = "0 FPS", metrics = {stick = Stick.R, prefW = 70, margin = {right = 8, top = 8}} }
                  }
               },
               Widget.Row{
                  name = "river-workspace", metrics = { stick = Stick.TLBR, flexW = 1, flexH = 1 },
                  kids = {
                     gutterLeft,
                     riverView,
                     liveTrace,
                     freqRuler
                  }
               }
            }
         }
      }
   }
   
   local sw, sh = 1280, 720
   if System.getWindowSize then
       local w, h = System.getWindowSize()
       if w and h then sw, sh = w, h end
   end
   rxTree:layout(0, 0, sw, sh)

   print("[Main] init() complete. The River workspace active.")
end

function update(dt)
   _G.frameCount = (_G.frameCount or 0) + 1
   animate.update(dt)
   
   fps = 1.0 / dt
   local fpsLabel = rxTree:findByName("fpsLabel")
   if fpsLabel then
      fpsLabel.props.text = string.format("%.0f FPS", fps)
   end

   AppController.sync()
   
   local riverEngine = _G.river or _G.waterfall
   if Hardware.isHardwareEnabled() then
      if _G.frameCount % 10 == 0 then AppController.pollState() end
      local hwSpec = Hardware.getSpectrum()
      if hwSpec and #hwSpec > 0 then 
         _G.lastSpectrumData = hwSpec
         if riverEngine and riverEngine.addRow then
            riverEngine.addRow(_G.lastSpectrumData)
         end
      end
   else
      if _G.frameCount % 2 == 0 then
         local dummy = {}
         for i = 1, 1024 do dummy[i] = -100 + 20 * math.sin(i / 50 + _G.frameCount/10) + 5 * math.random() end
         _G.lastSpectrumData = dummy
         if riverEngine and riverEngine.addRow then
            riverEngine.addRow(_G.lastSpectrumData)
         end
      end
   end
end
return UI
