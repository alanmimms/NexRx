--[[
   NexRx Application - Main Lua Entry Point
]]

local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path
io.stdout:setvbuf("no")
print("[Main] Script loading...")

local ui = require("ui.Widgets")
local uiState = require("ui.State")
local container = require("ui.Container")
_G.bands = require("Bands")
local bands = _G.bands
local Hardware = require("Hardware")
local Model = require("Model")
local AppController = require("AppController")
local events = require("Events")
local animate = require("Animate")
local keys = require("Keycodes")
local modeHelper = require("Modes")
local smeter = require("SMeter")
_G.calibration = require("Calibration")
local Edit = require("Edit")

-- Widget Classes
local Preselector = require("ui.Preselector")
local ISG = require("ui.ISG")
local AGC = require("ui.AGC")
local AudioUtils = require("ui.AudioUtils")
local CWUtils = require("ui.CWUtils")
local Panel = require("ui.Panel")
local SMeterWidget = require("ui.SMeter")
local ActiveTags = require("ui.ActiveTags")
local GraticuleLegend = require("ui.GraticuleLegend")
local SignalBox = require("ui.SignalBox")
local Button = require("ui.Button")
local Slider = require("ui.Slider")
local FrequencyDisplay = require("ui.FrequencyDisplay")
local Label = require("ui.Label")

local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0
local lastMouseX, lastMouseY = 0, 0
local BUTTON_NAMES = {"LEFT", "MIDDLE", "RIGHT"}
local activeTags = {}

-- Global widget collection
local allWidgets = {}
local rootWindow = nil

-- Global settings/constants
_G.sampleRate = 96000

local function getAllActiveTags()
   local allTags = {}
   if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then allTags["input.SHIFT"] = true end
   if activeTags["input.LCTRL"] or activeTags["input.RCTRL"] then allTags["input.CTRL"] = true end
   if activeTags["input.LALT"] or activeTags["input.RALT"] then allTags["input.ALT"] = true end

   for k, v in pairs(activeTags) do if v then allTags[k] = true end end
   if events and events.getModeTags then for _, t in ipairs(events.getModeTags()) do allTags[t] = true end end

   local mx, my = getMousePos()
   local hovered = events.getWidgetAt(mx, my)
   if hovered and hovered.id then allTags["state.Hovered:" .. hovered.id] = true end
   if activeTags["input.MouseLEFT"] and hovered and hovered.id then allTags["state.Pressed:" .. hovered.id] = true end
   local list = {}; for tag, _ in pairs(allTags) do table.insert(list, tag) end
   table.sort(list)
   return list
end

print("[Main] Loading configurations...")
local configFiles = {
   "config/default.lua",
   "config/settings.lua",
   "config/bands.lua",
   "config/colormaps.lua",
   "config/events.lua",
   "config/constraints.lua"
}
for _, file in ipairs(configFiles) do setbox.loadFile(file) end

local spectrumData = {}
_G.rxStats = { rms=0, gain0=0, phase0=0, gain1=0, phase1=0, w0_mag=0, w1_mag=0 }
_G.isgFreqEntryText = ""; _G.isgFreqEntryCursor = 0
_G.freqEntryText = ""; _G.freqEntryCursor = 0

function init()
   print("[Main] init() starting...")
   AppController.init()
   _G.calibration.init()
   
   local wfBins = 1024; local wfRows = 256; for i = 1, wfBins do spectrumData[i] = -100 end
   setClearColor(0.1, 0.1, 0.15)
   
   if audio.isInitialized() then audio.start() end
   if waterfall.init(wfBins, wfRows) then
      waterfall.setRange(Model.waterfall.minDB:get(), Model.waterfall.maxDB:get())
      if _G.colormaps and _G.colormaps[Model.waterfall.colormap:get()] then waterfall.setColormapData(_G.colormaps[Model.waterfall.colormap:get()]) end
      Hardware.enableWaterfall()
   end
   
   bands.init(); events.init(); ui.setEventsModule(events)
   if hw.connect("127.0.0.1", 5000, 5001) then Hardware.enableHardware() end
   
   -- Instantiate unified widget hierarchy
   rootWindow = Panel.new(); rootWindow.id = "id-root-window"; rootWindow.tags = {"widget.Window"}; table.insert(allWidgets, rootWindow)
   
   local mainArea = Panel.new(); mainArea.id = "main-area"; table.insert(allWidgets, mainArea)
   
   local leftPanel = Panel.new(); leftPanel.id = "left-sidebar"; leftPanel.tags = {"widget.Sidebar", "widget.LeftSidebar"}
   table.insert(allWidgets, leftPanel)
   
   local centerArea = Panel.new(); centerArea.id = "center-area"; centerArea.tags = {"widget.CenterArea"}
   table.insert(allWidgets, centerArea)
   
   local rightPanel = Panel.new(); rightPanel.id = "right-sidebar"; rightPanel.tags = {"widget.Sidebar", "widget.RightSidebar"}
   table.insert(allWidgets, rightPanel)
   
   local tagsPanel = Panel.new(); tagsPanel.id = "active-tags"; tagsPanel.tags = {"widget.Sidebar", "widget.ActiveTagsSidebar"}
   table.insert(allWidgets, tagsPanel)

   -- Sidebar Content (Left)
   local sm = SMeterWidget.new(); sm.id = "id-smeter"; table.insert(allWidgets, sm)
   local rf = FrequencyDisplay.new({ valueObs = Model.rx.VFO.activeValue }); rf.id = "id-rx-freq"; table.insert(allWidgets, rf)
   local rs = Slider.new({ valueObs = Model.rx.VFO.activeValue }); rs.id = "id-rx-slider"; table.insert(allWidgets, rs)
   local rv = Slider.new({ valueObs = Model.rx.volume.DB }); rv.id = "id-rx-vol"; table.insert(allWidgets, rv)
   local rg = Slider.new({ valueObs = Model.rx.RF.gainDB }); rg.id = "id-rx-gain"; table.insert(allWidgets, rg)

   -- Mode Buttons
   local modes = {"LSB", "USB", "AM", "CW", "FM"}
   for _, m in ipairs(modes) do
      local b = Button.new({ label = m, onClick = function() Model.set("rx.selectedMode", m) end })
      b.id = "id-rx-mode-" .. m; table.insert(allWidgets, b)
   end

   -- Band Buttons
   local bandsList = {"160m","80m","40m","20m","15m","10m"}
   for _, bName in ipairs(bandsList) do
      local b = Button.new({ label = bName, onClick = function() Model.set("rx.selectedBand", bName) end })
      b.id = "id-rx-band-" .. bName; table.insert(allWidgets, b)
   end

   -- Spectrum + Waterfall
   local spec = { 
      id = "id-spec", tags = {"widget.Spectrum", "widget.VFOControl"},
      gl = GraticuleLegend.new(), sb = SignalBox.new(),
      draw = function(self, id, x, y, w, h, parentLWC)
         Hardware.renderSpectrum(spectrumData, x, y, w, h)
         local span = _G.sampleRate / (Model.waterfall.zoom:get() or 1.0)
         self.gl:draw("spec-legend", x + 10, y + 10, 100, 45, parentLWC, string.format("%.1f kHz/div", span/10000), "20 dB/div")
         events.registerWidget(self.id, {x=x, y=y, w=w, h=h}, self.tags)
         
         local startF = Model.spectrumCenterFreq:get() - span/2
         local boxes = Model.signalBoxes:get(); local selIdx = Model.selectedSignalBoxIndex:get()
         for i, box in ipairs(boxes) do
            local x1 = x + (box.frequency - box.bandwidth/2 - startF) / span * w
            local x2 = x + (box.frequency + box.bandwidth/2 - startF) / span * w
            local boxTags = (i == selIdx) and {"state.Selected"} or {}
            self.sb:draw("sb"..i, x1, y, x2-x1, h, parentLWC, tostring(box.id), boxTags)
         end
      end
   }
   table.insert(allWidgets, spec)

   local wf = {
      id = "id-wf", tags = {"widget.Waterfall", "widget.VFOControl"},
      draw = function(self, id, x, y, w, h, parentLWC)
         Hardware.renderWaterfall(x, y, w, h, Model.waterfall.zoom:get(), 0.5)
         events.registerWidget(self.id, {x=x, y=y, w=w, h=h}, self.tags)
      end
   }
   table.insert(allWidgets, wf)

   -- Other widgets
   local tagsWidget = ActiveTags.new(); tagsWidget.id = "id-active-tags"; table.insert(allWidgets, tagsWidget)

   Edit.init(events)
   print("[Main] init() complete.")
end

function update(dt)
   animate.update(dt)
   frameCount = frameCount + 1; fpsAccum = fpsAccum + dt; fpsFrames = fpsFrames + 1
   if fpsAccum >= 1.0 then fps = fpsFrames / fpsAccum; fpsAccum = 0; fpsFrames = 0 end

   -- Input handling (simplified)
   local scancodes = keys.getAllScancodes()
   for _, sc in ipairs(scancodes) do
      local isDown = isKeyDown(sc); local n = keys.getName(sc)
      if n then
         local prevDown = activeTags["input."..n]; activeTags["input."..n] = isDown or nil
         if isDown and not prevDown then
            events.dispatch(events.createEvent(events.Type.KEY_DOWN, {key=n, scancode=sc, modifiers=getAllActiveTags()}))
         elseif not isDown and prevDown then
            events.dispatch(events.createEvent(events.Type.KEY_UP, {key=n, scancode=sc, modifiers=getAllActiveTags()}))
         end
      end
   end

   local mx, my = getMousePos(); local tags = getAllActiveTags()
   setbox.setActiveTags(tags); uiState.beginFrame()

   local wheel = getMouseWheel()
   if wheel ~= 0 then events.dispatch(events.createEvent(events.Type.MOUSE_WHEEL, {x=mx,y=my,delta=wheel,modifiers=tags})) end
   for b = 0, 2 do
      if isMouseClicked(b) then events.dispatch(events.createEvent(events.Type.MOUSE_DOWN, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=tags}))
      elseif isMouseReleased(b) then events.dispatch(events.createEvent(events.Type.MOUSE_UP, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=tags})) end
   end

   AppController.sync()
   if Hardware.isHardwareEnabled() then
      if frameCount % 10 == 0 then AppController.pollState() end
      local hwSpec = Hardware.getSpectrum()
      if hwSpec and #hwSpec > 0 then spectrumData = hwSpec; Hardware.updateWaterfall(spectrumData) end
   else Hardware.updateWaterfall(spectrumData) end
end

function draw()
   local winW, winH = getWindowSize()
   local tags = getAllActiveTags()
   events.clearWidgets()
   ui.beginFrame()
   setbox.setActiveTags(tags)
   
   -- Solve layout for all widgets
   local regions = container.solveAll(rootWindow, allWidgets, winW, winH)
   if not regions then return end

   -- Draw recursively
   local function recursiveDraw(w, parentLWC)
      local r = regions[w.id]
      if not r then return end
      
      local lwc = setbox.newContext(w.tags or {}, parentLWC)
      
      -- Draw based on widget type/id
      if w.id == "id-smeter" then
         w:draw(w.id, r.x, r.y, r.w, r.h, lwc, smeter.getReading())
      elseif w.draw then
         w:draw(w.id, r.x, r.y, r.w, r.h, lwc)
      end

      -- Find and draw children
      local children = {}
      for _, other in ipairs(allWidgets) do
         local pId = other._props and other._props.parent
         if pId == w.id then table.insert(children, other) end
      end
      table.sort(children, function(a, b) return (a._props.order or 0) < (b._props.order or 0) end)
      
      for _, child in ipairs(children) do recursiveDraw(child, lwc) end
   end

   recursiveDraw(rootWindow, nil)

   -- Top bar overlay
   local rT = regions["top-bar"]
   if rT then
      drawRect(rT.x, rT.y, rT.w, rT.h, 0.08, 0.08, 0.12, 1.0)
      drawText(rT.x + 10, rT.y + 8, string.format("NexRx | %.0f FPS", fps), 0.6, 0.7, 0.9, 1.0)
   end

   if Edit.isEditModifierHeld(tags) then Edit.drawHandles(getMousePos(), events.getWidgetAt(getMousePos()), true) end
   ui.endFrame(); uiState.endFrame()
end
