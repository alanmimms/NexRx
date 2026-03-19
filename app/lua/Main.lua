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

_G.sampleRate = 96000
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

  -- if frameInput.mouseClicked then print("[Main] renderUI Clicked at " .. frameInput.mouseX .. "," .. frameInput.mouseY) end

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
  frameInput.mouseX, frameInput.mouseY = x, y
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  events.dispatch(events.createEvent(events.Type.MOUSE_MOVE, { x=x, y=y }))
end

local function onMouseEvent(type, x, y, button, isDown, mods)
  frameInput.mouseX, frameInput.mouseY = x, y
  if type == "button" then
    frameInput.mouseDown = isDown
    if isDown then 
        frameInput.mouseClicked = true 
        print("[Main] MouseClicked at " .. x .. "," .. y)
    else 
        frameInput.mouseReleased = true 
    end
  elseif type == "wheel" then
    frameInput.mouseWheel = button -- delta
  end

  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  
  -- 1. Dispatch to Widget hierarchy (for BridgeWidgets/etc)
  local handled = false
  if hit and hit.handleEvent then
    handled = hit:handleEvent({
	type = (type == "wheel") and "mouseWheel" or "mouseButton",
	button = button,
	isDown = isDown,
	x = x, y = y,
	delta = type == "wheel" and button or 0, -- Raylib/Bridge mapping for wheel
	mods = mods
    })
    if isDown then hit:setFocus() end
  end

  -- 2. Dispatch to global events system if not handled (or for non-widget targets)
  if not handled then
    local et = events.Type.MOUSE_MOVE
    if type == "button" then et = isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP 
    elseif type == "wheel" then et = events.Type.MOUSE_WHEEL end
    events.dispatch(events.createEvent(et, { x=x, y=y, button=button == 0 and "LEFT" or (button == 1 and "MIDDLE" or "RIGHT"), delta = type == "wheel" and button or 0 }))
  end
end

local function onKeyEvent(key, isDown, mods)
  local focused = Widget.getFocused()
  local handled = false
  if focused and focused.handleEvent then
    handled = focused:handleEvent({type = "key", key = key, isDown = isDown, mods = mods})
  end
  
  if not handled then
    -- Map Raylib key to Events Type
    local et = isDown and events.Type.KEY_DOWN or events.Type.KEY_UP
    -- Note: mapping raylib 'key' to SDL scancodes/names might be needed here 
    -- for full events.lua compatibility. For now we pass it through.
    events.dispatch(events.createEvent(et, { key = key, isDown = isDown, mods = mods }))
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

   -- Instantiate Widgets for bridging
   local sMeter = SMeter.new()
   local freqDisplay = FrequencyDisplay.new({ valueObs = Model.rx.VFO.activeValue })
   local vfoSlider = Slider.new({ valueObs = Model.rx.VFO.activeValue })
   local volSlider = Slider.new({ valueObs = Model.rx.volume.DB })
   local rfGainSlider = Slider.new({ valueObs = Model.rx.RF.gainDB })
   local activeTags = ActiveTags.new()

   local modeSlider = Slider.DiscreteSlider.new({
      valueObs = Model.rx.selectedMode,
      stops = {
         {label = "LSB", value = "LSB"},
         {label = "USB", value = "USB"},
         {label = "AM",  value = "AM"},
         {label = "CW",  value = "CW"},
         {label = "FM",  value = "FM"}
      },
      onChanged = function(v) Model.set("rx.selectedMode", v) end
   })

   local bandSlider = Slider.DiscreteSlider.new({
      valueObs = Model.rx.selectedBand,
      stops = {
         {label = "160m", value = "160m"},
         {label = "80m",  value = "80m"},
         {label = "40m",  value = "40m"},
         {label = "20m",  value = "20m"},
         {label = "15m",  value = "15m"},
         {label = "10m",  value = "10m"}
      },
      onChanged = function(v) Model.set("rx.selectedBand", v) end
   })

   -- Sidebar
   local leftSidebar = Widget.Column{
      name = "left-sidebar", tags = {"widget.Sidebar", "widget.LeftSidebar"},
      metrics = { stick = Stick.TLB, prefW = 280, margin = {left=8, right=8, top=8, bottom=8} },
      kids = {
         Widget.BridgeWidget{ id = "id-rx-freq", orig = freqDisplay, metrics = { stick = Stick.TLR, prefH = 40 },
            drawArgs = { _G.freqEntryText, _G.freqEntryCursor, {"VFOControl"} }
         },
         Widget.BridgeWidget{ id = "id-rx-slider", orig = vfoSlider, metrics = { stick = Stick.TLR, prefH = 20 },
            onEvent = function(self, ev) 
               local et = events.Type.MOUSE_MOVE
               if ev.type == "mouseButton" then et = ev.isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP 
               elseif ev.type == "mouseWheel" then et = events.Type.MOUSE_WHEEL end
               return events.dispatch(events.createEvent(et, { x=ev.x, y=ev.y, delta=ev.delta, button=ev.button == 0 and "LEFT" or (ev.button == 1 and "MIDDLE" or "RIGHT") })) 
            end,
            drawArgs = { 0.1e6, 30.0e6, 14.2e6 } 
         },
         Widget.Label{text = "Mode", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.BridgeWidget{ id = "id-mode-slider", orig = modeSlider, metrics = { stick = Stick.TLR, prefH = 30 },
            onEvent = function(self, ev) 
               local et = events.Type.MOUSE_MOVE
               if ev.type == "mouseButton" then et = ev.isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP end
               return events.dispatch(events.createEvent(et, { x=ev.x, y=ev.y, button=ev.button == 0 and "LEFT" or "RIGHT" })) 
            end
         },
         Widget.Label{text = "Band", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.BridgeWidget{ id = "id-band-slider", orig = bandSlider, metrics = { stick = Stick.TLR, prefH = 30 },
            onEvent = function(self, ev) 
               local et = events.Type.MOUSE_MOVE
               if ev.type == "mouseButton" then et = ev.isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP end
               return events.dispatch(events.createEvent(et, { x=ev.x, y=ev.y, button=ev.button == 0 and "LEFT" or "RIGHT" })) 
            end
         },
         Widget.Label{text = "Volume", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.BridgeWidget{ id = "id-rx-vol", orig = volSlider, metrics = { stick = Stick.TLR, prefH = 20 },
            onEvent = function(self, ev) 
               local et = events.Type.MOUSE_MOVE
               if ev.type == "mouseButton" then et = ev.isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP 
               elseif ev.type == "mouseWheel" then et = events.Type.MOUSE_WHEEL end
               return events.dispatch(events.createEvent(et, { x=ev.x, y=ev.y, delta=ev.delta, button=ev.button == 0 and "LEFT" or (ev.button == 1 and "MIDDLE" or "RIGHT") })) 
            end,
            drawArgs = { -60, 0, 0 }
         },
         Widget.Label{text = "RF Gain", metrics = {stick = Stick.TLR, prefH = 20, margin={top=10}}},
         Widget.BridgeWidget{ id = "id-rx-gain", orig = rfGainSlider, metrics = { stick = Stick.TLR, prefH = 20 },
            onEvent = function(self, ev) 
               local et = events.Type.MOUSE_MOVE
               if ev.type == "mouseButton" then et = ev.isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP 
               elseif ev.type == "mouseWheel" then et = events.Type.MOUSE_WHEEL end
               return events.dispatch(events.createEvent(et, { x=ev.x, y=ev.y, delta=ev.delta, button=ev.button == 0 and "LEFT" or (ev.button == 1 and "MIDDLE" or "RIGHT") })) 
            end,
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
            onEvent = function(self, ev) 
               local et = events.Type.MOUSE_MOVE
               if ev.type == "mouseButton" then et = ev.isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP 
               elseif ev.type == "mouseWheel" then et = events.Type.MOUSE_WHEEL end
               return events.dispatch(events.createEvent(et, { x=ev.x, y=ev.y, delta=ev.delta, button=ev.button == 0 and "LEFT" or (ev.button == 1 and "MIDDLE" or "RIGHT") })) 
            end,
            orig = {
               id = "id-spec", tags = {"widget.Spectrum", "widget.VFOControl"},
               gl = GraticuleLegend.new(), sb = SignalBox.new(),
               draw = function(self, id, x, y, w, h, parentLWC)
                  Hardware.renderSpectrum(spectrumData, x, y, w, h)
                  
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
                     local boxX = x + (w / 2) + ((freq - center) / span) * w - (boxW / 2)
                     
                     local isSelected = (i == selectedIdx)
                     local tags = {"widget.SignalBox"}
                     if isSelected then table.insert(tags, "state.Selected") end
                     if box.ghost then table.insert(tags, "state.Ghost") end
                     
                     local label = box.name or tostring(box.id)
                     if isSelected and events.hasModeTag("state.SbNamingMode") then
                        label = _G.sbNamingText .. "|"
                     end
                     
                     self.sb:draw("sb-" .. box.id, boxX, y, boxW, h, parentLWC, label, tags, { index = i })
                  end
                  
                  self.gl:draw("spec-legend", x + 10, y + 10, 100, 45, parentLWC, string.format("%.1f kHz/div", span/10000), "20 dB/div")
                  events.registerWidget(self.id, {x=x, y=y, w=w, h=h}, self.tags)
               end
            }
         },
         Widget.BridgeWidget{
            id = "id-wf", tags = {"widget.Waterfall", "widget.VFOControl"},
            metrics = { stick = Stick.TLBR, flexH = 1, minH = 200 },
            onEvent = function(self, ev) 
               local et = events.Type.MOUSE_MOVE
               if ev.type == "mouseButton" then et = ev.isDown and events.Type.MOUSE_DOWN or events.Type.MOUSE_UP 
               elseif ev.type == "mouseWheel" then et = events.Type.MOUSE_WHEEL end
               return events.dispatch(events.createEvent(et, { x=ev.x, y=ev.y, delta=ev.delta, button=ev.button == 0 and "LEFT" or (ev.button == 1 and "MIDDLE" or "RIGHT") })) 
            end,
            orig = {
               id = "id-wf", tags = {"widget.Waterfall", "widget.VFOControl"},
               draw = function(self, id, x, y, w, h, parentLWC)
                  Hardware.renderWaterfall(x, y, w, h, Model.waterfall.zoom:get(), 0.5)
                  
                  -- Render SignalBox highlights on waterfall too
                  local boxes = Model.signalBoxes:get()
                  local selectedIdx = Model.selectedSignalBoxIndex:get()
                  local zoom = Model.waterfall.zoom:get() or 1.0
                  local span = _G.sampleRate / zoom
                  local center = Model.spectrumCenterFreq:peek()

                  for i, box in ipairs(boxes) do
                     local bw = box.bandwidth
                     local freq = box.frequency
                     local boxW = (bw / span) * w
                     local boxX = x + (w / 2) + ((freq - center) / span) * w - (boxW / 2)
                     
                     local alpha = (i == selectedIdx) and 0.2 or 0.1
                     local r, g, b = 1.0, 1.0, 0.0 -- Yellow
                     drawRect(boxX, y, boxW, h, r, g, b, alpha)
                  end
                  
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
         Widget.BridgeWidget{ id = "id-smeter", orig = sMeter, metrics = { stick = Stick.TLR, prefH = 66 }, 
            onEvent = function(self, ev) return false end,
            drawArgs = {} 
         },
         Widget.BridgeWidget{ id = "id-active-tags", orig = activeTags, metrics = { flexH = 1 } }
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
                     Widget.Label{ id = "titleLabel", text = "NexRx SDR", metrics = {stick = Stick.L, flexW = 1} },
                     Widget.Label{ id = "fpsLabel", text = "0 FPS", metrics = {stick = Stick.R, prefW = 100} }
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

   -- Update BridgeWidget drawArgs for dynamic components
   local smW = rxTree:findByName("id-smeter")
   if smW then smW.drawArgs = { SMeter.getReading() } end

   local freqW = rxTree:findByName("id-rx-freq")
   if freqW then freqW.drawArgs = { _G.freqEntryText, _G.freqEntryCursor, {"VFOControl"} } end

   local sliderW = rxTree:findByName("id-rx-slider")
   if sliderW then sliderW.drawArgs = { 0.1e6, 30.0e6, Model.rx.VFO.activeValue:peek() } end

   local volW = rxTree:findByName("id-rx-vol")
   if volW then volW.drawArgs = { -60, 0, Model.rx.volume.DB:peek() } end

   local gainW = rxTree:findByName("id-rx-gain")
   if gainW then gainW.drawArgs = { -20, 60, Model.rx.RF.gainDB:peek() } end
end
