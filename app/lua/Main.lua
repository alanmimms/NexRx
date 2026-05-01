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

_G.lowestFreq = 100.0e3
_G.highestFreq = 30.0e6
_G.sampleRate = 96.0e3
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

local function onMouseMove_inner(x, y)
  frameInput.mouseX, frameInput.mouseY = x, y
  if not rxTree then return end
  
  -- SDL2/raylib usually don't send mods in mouse move, so we keep what we have
  -- but if we want to be safe we can use a C++ hook or just rely on last known.

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

local function translateMods(mods)
  local t = {}
  if mods then
    if (mods & 1) ~= 0 then table.insert(t, "input.SHIFT") end
    if (mods & 2) ~= 0 then table.insert(t, "input.CTRL") end
    if (mods & 4) ~= 0 then table.insert(t, "input.ALT") end
  end
  return t
end

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
  
  System.printVersion();


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

   local vfoDisp = FrequencyDisplay{ id = "id-rx-freq", valueObs = Model.rx.VFO.activeValue, tags = {"widget.VFOControl"}, metrics = { stick = "TLR", prefH = 40 } }
   local spec = Spectrum{ id = "id-spec", tags = {"widget.Spectrum", "widget.VFOControl"}, metrics = { stick = "TLBR", flexH = 1, minH = 150 }, eventRedirect = vfoDisp }
   local wf = Waterfall{ id = "id-wf", tags = {"widget.Waterfall", "widget.VFOControl"}, metrics = { stick = "TLBR", flexH = 1, minH = 200 }, eventRedirect = vfoDisp }

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
                           vfoDisp,
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
                           spec,
                           wf
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
   
   local sw, sh = 1280, 720
   if System.getWindowSize then
       local w, h = System.getWindowSize()
       if w and h then sw, sh = w, h end
   end
   rxTree:layout(0, 0, sw, sh)

   print("[Main] init() complete.")
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
   
   if Hardware.isHardwareEnabled() then
      if _G.frameCount % 10 == 0 then AppController.pollState() end
      local hwSpec = Hardware.getSpectrum()
      if hwSpec and #hwSpec > 0 then 
         _G.lastSpectrumData = hwSpec
         Hardware.updateWaterfall(_G.lastSpectrumData) 
      end
   else
      if _G.frameCount % 2 == 0 then
         local dummy = {}
         for i = 1, 1024 do dummy[i] = -100 + 20 * math.sin(i / 50 + _G.frameCount/10) + 5 * math.random() end
         _G.lastSpectrumData = dummy
         Hardware.updateWaterfall(_G.lastSpectrumData)
      end
   end
end
return UI
