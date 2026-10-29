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
local riverViewRef = nil
local freqRulerRef = nil
local spectrumTimer = 0.0
local spectrumInterval = 1.0 / 30.0 -- 30 Hz spectrum slice rate (33.333 ms)

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
    eventData.button = (button == 0 and "LEFT") or (button == 1 and "RIGHT") or (button == 2 and "MIDDLE") or "RIGHT"
  end

  local activeId = uiState.getActive()
  local activeWidget = activeId and rxTree:findByID(activeId)
  
  local hit = Widget.updateGlobalMouse(rxTree, x, y)
  local target = (type ~= "wheel" and activeWidget) or hit or rxTree
  
  if freqRulerRef and (freqRulerRef.contextMenuOpen or freqRulerRef.bandMenuOpen) and type == "button" and isDown then
    if target ~= freqRulerRef then
      freqRulerRef.contextMenuOpen = false
      freqRulerRef.bandMenuOpen = false
      freqRulerRef.tuneInputActive = false
    end
  end

  local prevFocus = Widget.getFocused()
  local handled = target:handleEvent(eventData)
  if handled and isDown and type == "button" then 
    if Widget.getFocused() == prevFocus then
      target:setFocus() 
    end
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

  -- If FrequencyRuler popup text input is open, route keys directly to it first
  if freqRulerRef and (freqRulerRef.contextMenuOpen or freqRulerRef.bandMenuOpen) then
    if freqRulerRef:handleEvent(eventData) then
      return
    end
  end

  -- DEL key deletes focused SignalBox
  if isDown and (keyName == "DELETE" or key == 261) and riverViewRef then
    if riverViewRef:removeFocusedSignalBox() then
      return
    end
  end

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
      riverEngine.init(1024, 1800)
      riverEngine.setRange(Model.waterfall.minDB:get(), Model.waterfall.maxDB:get())
      if _G.colormaps and _G.colormaps[Model.waterfall.colormap:get()] then 
         riverEngine.setColormapData(_G.colormaps[Model.waterfall.colormap:get()]) 
      end
   end

   local initialVFO = Model.rx.VFO.activeValue:get() or 14.200e6
   local initialBand = _G.bands and _G.bands.getBand(Model.rx.selectedBand:get() or "20m")
   local initCenter = Model.spectrumCenterFreq:peek() or initialVFO
   local initFMin = initCenter - 150e3
   local initFMax = initCenter + 150e3
   local autoMode, autoBw, autoLabel, autoCaller = RiverView:getAutoSignalDef(initialVFO)

   local riverView = RiverView{
      id = "id-river-view",
      tags = {"widget.RiverView"},
      timeSpan = 30.0,
      timeOffset = 0.0,
      freqMin = initFMin,
      freqMax = initFMax,
      metrics = { stick = Stick.TLBR, flexW = 1, flexH = 1 },
      signalBoxes = {
         {
            id = "id-primary-box",
            index = 1,
            mode = autoMode or Model.rx.selectedMode:get() or "USB",
            carrierFreq = initialVFO,
            bandwidth = autoBw or 2800,
            label = autoLabel or "CQ",
            caller = autoCaller
         }
      },
      decoderTracks = {
         { freq = 14.074e6, mode = "FT8", callsign = "K1JT", text = "CQ K1JT FN20" },
         { freq = 14.075e6, mode = "FT8", callsign = "W3LPL", text = "K1JT W3LPL -08" },
         { freq = 14.025e6, mode = "CW", callsign = "W1AW", text = "CQ CQ DE W1AW K" },
         { freq = 14.225e6, mode = "USB", callsign = "N2AA", text = "59 IN NY 73" }
      }
   }
   riverViewRef = riverView

   local freqRuler = FrequencyRuler{
      id = "id-freq-ruler",
      tags = {"widget.FrequencyRuler"},
      freqMin = initFMin,
      freqMax = initFMax,
      metrics = { stick = Stick.TRB, prefW = 60, flexH = 1 }
   }
   freqRulerRef = freqRuler

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
      boxes = riverView.signalBoxes,
      metrics = { stick = Stick.TLB, prefW = 84, flexH = 1 }
   }

   local function updateFrequencyRange(fMin, fMax, isFinal)
      fMin = math.max(1.8e6, fMin)
      fMax = math.min(30.0e6, fMax)
      riverView.freqMin = fMin
      riverView.freqMax = fMax
      riverView:layout(riverView.props.x or 0, riverView.props.y or 0, riverView.props.w or 800, riverView.props.h or 450)

      freqRuler.props.freqMin = fMin
      freqRuler.props.freqMax = fMax

      gutterLeft.props.freqMin = fMin
      gutterLeft.props.freqMax = fMax

      local fMinNorm, fMaxNorm = riverView:getNormalizedFreqRange()
      liveTrace.props.freqMinNorm = fMinNorm
      liveTrace.props.freqMaxNorm = fMaxNorm
   end

   local isUpdatingBandFromFreq = false
   local lastHwCenter = Model.spectrumCenterFreq:peek() or 14.2e6
   Model.spectrumCenterFreq:watch(function(newCenter)
      local deltaHz = newCenter - lastHwCenter
      lastHwCenter = newCenter
      if math.abs(deltaHz) > 1.0 then
         if Hardware.isHardwareEnabled() then
            Hardware.setVFO(newCenter)
         end
         if _G.river and _G.river.shiftFrequency then
            _G.river.shiftFrequency(deltaHz, _G.sampleRate or 384e3)
         end
         if _G.bands and _G.bands._getBandForFreq then
            local bName = _G.bands._getBandForFreq(newCenter)
            if bName and bName ~= "OOB" and Model.rx.selectedBand:peek() ~= bName then
               isUpdatingBandFromFreq = true
               Model.rx.selectedBand:set(bName)
               isUpdatingBandFromFreq = false
            end
         end

         local hwMin = newCenter - 150e3
         local hwMax = newCenter + 150e3
         if riverView.freqMax > hwMax or riverView.freqMin < hwMin then
            local curSpan = riverView.freqMax - riverView.freqMin
            local fMin = math.max(hwMin, math.min(hwMax - curSpan, riverView.freqMin))
            local fMax = fMin + curSpan
            updateFrequencyRange(fMin, fMax, true)
         elseif riverView and riverView.layout then
            riverView:layout(riverView.props.x or 0, riverView.props.y or 0, riverView.props.w or 800, riverView.props.h or 450)
         end
      end
   end)

   local function syncDSPToActiveSignalBox()
      local curIdx = Model.selectedSignalBoxIndex:get() or 1
      local activeBox = riverView and riverView.signalBoxes and riverView.signalBoxes[curIdx]
      if not activeBox then return end
      local curCenter = Model.spectrumCenterFreq:peek() or 14.2e6
      local carrier = activeBox.carrierFreq or curCenter
      local tuneOffsetHz = carrier - curCenter

      if _G.rx and _G.rx.setTuningOffset then
         _G.rx.setTuningOffset(tuneOffsetHz)
      end

      local mode = activeBox.mode or "USB"
      local modeId = 0
      if mode == "USB" then modeId = 0
      elseif mode == "LSB" then modeId = 1
      elseif mode == "AM" then modeId = 2
      elseif mode == "CW" then modeId = 3
      end
      if _G.rx and _G.rx.setModeId then
         _G.rx.setModeId(modeId)
      end

      local bw = activeBox.bandwidth or 2800
      local center = 0.0
      if mode == "USB" then
         center = bw * 0.5
      elseif mode == "LSB" then
         center = -bw * 0.5
      elseif mode == "CW" then
         center = (Model.rx and Model.rx.CW and Model.rx.CW.pitch and Model.rx.CW.pitch:peek()) or 700.0
      elseif mode == "AM" then
         center = 0.0
      end
      if _G.rx and _G.rx.setBandpassCenter then
         _G.rx.setBandpassCenter(center)
      end
      if _G.rx and _G.rx.setBandpassWidth then
         _G.rx.setBandpassWidth(bw)
      end
   end

   riverView.onSignalBoxesChanged = function(self, boxes)
      gutterLeft.activeBoxes = boxes
      syncDSPToActiveSignalBox()
   end

   riverView.onFrequencyChange = function(self, fMin, fMax, isFinal)
      updateFrequencyRange(fMin, fMax, isFinal)
   end

   freqRuler.onFrequencyChange = function(self, fMin, fMax, isFinal)
      updateFrequencyRange(fMin, fMax, isFinal)
   end

   freqRuler.onSelectBand = function(self, bandName)
      local b = _G.bands and _G.bands.getBand(bandName)
      if b then
         local bLower = b.lowerEdge or b.startHz
         local bUpper = b.upperEdge or b.endHz
         local bandSpan = bUpper - bLower
         local centerFreq
         if bandSpan > 300e3 then
            centerFreq = bLower + 150e3
         else
            centerFreq = (bLower + bUpper) * 0.5
         end

         isUpdatingBandFromFreq = true
         Model.rx.selectedBand:set(bandName)
         isUpdatingBandFromFreq = false
         Model.spectrumCenterFreq:set(centerFreq)
         updateFrequencyRange(centerFreq - 150e3, centerFreq + 150e3, true)
         riverView:tuneActiveSignalBox(centerFreq)
      end
   end

   freqRuler.onSetCenterFrequency = function(self, targetFreq)
      local f = math.max(1.8e6 + 150e3, math.min(30.0e6 - 150e3, targetFreq))
      if _G.bands and _G.bands._getBandForFreq then
         local bName = _G.bands._getBandForFreq(f)
         if bName and bName ~= "OOB" then
            isUpdatingBandFromFreq = true
            Model.rx.selectedBand:set(bName)
            isUpdatingBandFromFreq = false
         end
      end
      Model.spectrumCenterFreq:set(f)
      updateFrequencyRange(f - 150e3, f + 150e3, true)
      riverView:tuneActiveSignalBox(f)
   end
   freqRuler.onManualTune = freqRuler.onSetCenterFrequency

   freqRuler.onZoomPreset = function(self, presetHz)
      local curCenter = Model.spectrumCenterFreq:peek() or 14.2e6
      local hwMin = curCenter - 150e3
      local hwMax = curCenter + 150e3
      local span = math.max(10e3, math.min(300e3, presetHz))
      local curVisCenter = (riverView.freqMin + riverView.freqMax) * 0.5
      local fMin = curVisCenter - span * 0.5
      local fMax = curVisCenter + span * 0.5
      if fMin < hwMin then
         fMin = hwMin
         fMax = fMin + span
      elseif fMax > hwMax then
         fMax = hwMax
         fMin = fMax - span
      end
      updateFrequencyRange(fMin, fMax, true)
   end

   freqRuler.onCenterVFO = function(self)
      local curIdx = Model.selectedSignalBoxIndex:get() or 1
      local activeBox = riverView.signalBoxes and riverView.signalBoxes[curIdx]
      local vfoFreq = activeBox and activeBox.carrierFreq or Model.get("rx.VFO.activeValue")
      if vfoFreq then
         Model.spectrumCenterFreq:set(vfoFreq)
         updateFrequencyRange(vfoFreq - 150e3, vfoFreq + 150e3, true)
      end
   end

   gutterLeft.onCenterFrequency = function(self, targetFreq)
      local curCenter = Model.spectrumCenterFreq:peek() or 14.2e6
      local hwMin = curCenter - 150e3
      local hwMax = curCenter + 150e3
      local span = riverView.freqMax - riverView.freqMin
      local newMin = math.max(hwMin, math.min(hwMax - span, targetFreq - span * 0.5))
      local newMax = newMin + span
      updateFrequencyRange(newMin, newMax, true)
   end

   gutterLeft.onTopPileClick = function(self)
      local targetFreq = nil
      local minAbove = math.huge
      for _, box in ipairs(self.topPileBoxes or {}) do
         local f = box.carrierFreq or box.freq or 0
         if f < minAbove then minAbove = f; targetFreq = f end
      end
      for _, a in ipairs(self.topPileAlerts or {}) do
         local f = a.freq or 0
         if f < minAbove then minAbove = f; targetFreq = f end
      end
      if targetFreq then
         self:onCenterFrequency(targetFreq)
      else
         local curCenter = Model.spectrumCenterFreq:peek() or 14.2e6
         local hwMax = curCenter + 150e3
         local span = riverView.freqMax - riverView.freqMin
         local newMax = math.min(hwMax, riverView.freqMax + span * 0.5)
         local newMin = newMax - span
         updateFrequencyRange(newMin, newMax, true)
      end
   end

   gutterLeft.onBottomPileClick = function(self)
      local targetFreq = nil
      local maxBelow = -math.huge
      for _, box in ipairs(self.bottomPileBoxes or {}) do
         local f = box.carrierFreq or box.freq or 0
         if f > maxBelow then maxBelow = f; targetFreq = f end
      end
      for _, a in ipairs(self.bottomPileAlerts or {}) do
         local f = a.freq or 0
         if f > maxBelow then maxBelow = f; targetFreq = f end
      end
      if targetFreq then
         self:onCenterFrequency(targetFreq)
      else
         local curCenter = Model.spectrumCenterFreq:peek() or 14.2e6
         local hwMin = curCenter - 150e3
         local span = riverView.freqMax - riverView.freqMin
         local newMin = math.max(hwMin, riverView.freqMin - span * 0.5)
         local newMax = newMin + span
         updateFrequencyRange(newMin, newMax, true)
      end
   end

   gutterLeft.onSelectBox = function(self, idx)
      if riverView and riverView.signalBoxes and riverView.signalBoxes[idx] then
         for i, b in ipairs(riverView.signalBoxes) do
            if i ~= idx and b.stopPlayback then b:stopPlayback() end
         end
         local box = riverView.signalBoxes[idx]
         box:setFocus()
         Model.set("rx.VFO.activeValue", box.carrierFreq)
         Model.set("rx.selectedMode", box.mode)
         riverView:layout(riverView.props.x or 0, riverView.props.y or 0, riverView.props.w or 800, riverView.props.h or 450)
      end
   end

   -- Reactive synchronization between VFO, Mode, Band, and The River
   Model.rx.VFO.activeValue:watch(function(freq)
      if riverView and freq then
         local activeIdx = Model.selectedSignalBoxIndex:get() or 1
         if riverView.signalBoxes and riverView.signalBoxes[activeIdx] then
            riverView.signalBoxes[activeIdx].carrierFreq = freq
         end
         if gutterLeft and gutterLeft.activeBoxes and gutterLeft.activeBoxes[activeIdx] then
            gutterLeft.activeBoxes[activeIdx].carrierFreq = freq
         end
      end
      syncDSPToActiveSignalBox()
   end)

   Model.rx.selectedMode:watch(function(mode)
      if riverView and mode then
         local activeIdx = Model.selectedSignalBoxIndex:get() or 1
         if riverView.signalBoxes and riverView.signalBoxes[activeIdx] then
            riverView.signalBoxes[activeIdx].mode = mode
         end
      end
      syncDSPToActiveSignalBox()
   end)

   Model.selectedSignalBoxIndex:watch(function(idx)
      if riverView and riverView.signalBoxes and idx then
         for i, b in ipairs(riverView.signalBoxes) do
            if i ~= idx and b.stopPlayback then
               b:stopPlayback()
            end
         end
         local box = riverView.signalBoxes[idx]
         if box then
            box:setFocus()
            Model.set("rx.VFO.activeValue", box.carrierFreq)
            Model.set("rx.selectedMode", box.mode)
         end
      end
      syncDSPToActiveSignalBox()
   end)

   Model.rx.selectedBand:watch(function(bandName)
      if isUpdatingBandFromFreq then return end
      if _G.bands and bandName then
         local b = _G.bands.getBand(bandName)
         if b and riverView then
            local bLower = b.lowerEdge or b.startHz
            local bUpper = b.upperEdge or b.endHz
            local curCenter = Model.spectrumCenterFreq:peek() or 14.2e6
            if curCenter >= bLower and curCenter <= bUpper then
               return
            end
            local bandSpan = bUpper - bLower
            local centerFreq
            if bandSpan > 300e3 then
               centerFreq = bLower + 150e3
            else
               centerFreq = (bLower + bUpper) * 0.5
            end
            if Model.spectrumCenterFreq:peek() ~= centerFreq then
               Model.spectrumCenterFreq:set(centerFreq)
               updateFrequencyRange(centerFreq - 150e3, centerFreq + 150e3, true)
               riverView:tuneActiveSignalBox(centerFreq)
            end
         end
      end
   end)

   -- Initial synchronization across all River widgets
   updateFrequencyRange(initFMin, initFMax)
   syncDSPToActiveSignalBox()

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
                  interactive = false,
                  kids = {
                     Widget.Label{ id = "titleLabel", text = "NexRx SDR | The River", metrics = {stick = Stick.L, prefW = 180, margin = {left = 12, top = 8}} },
                     Widget.Spacer{ name = "top-bar-spacer", metrics = { stick = Stick.L, flexW = 1 } },
                     Widget.Label{ text = "Vol:", metrics = {stick = Stick.R, prefW = 32, margin = {right = 4, top = 8}} },
                     Slider{ id = "id-rx-vol", valueObs = Model.rx.volume.DB, minVal = -60, maxVal = 0, metrics = { stick = Stick.R, prefW = 140, prefH = 20, margin = {right = 16, top = 9}} },
                     Widget.Label{ text = "RF Gain:", metrics = {stick = Stick.R, prefW = 56, margin = {right = 4, top = 8}} },
                     Slider{ id = "id-rx-gain", valueObs = Model.rx.RF.gainDB, minVal = -20, maxVal = 60, metrics = { stick = Stick.R, prefW = 140, prefH = 20, margin = {right = 16, top = 9}} },
                     Widget.Label{ id = "fpsLabel", text = "0 FPS", metrics = {stick = Stick.R, prefW = 60, margin = {right = 10, top = 8}} }
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

   if riverViewRef and riverViewRef.getNormalizedFreqRange then
      local fMinNorm, fMaxNorm = riverViewRef:getNormalizedFreqRange()
      local liveTrace = rxTree and rxTree:findByName("id-live-trace")
      if liveTrace then
         liveTrace.props.freqMinNorm = fMinNorm
         liveTrace.props.freqMaxNorm = fMaxNorm
      end
   end
   
   if Hardware.isHardwareEnabled() then
      if _G.frameCount % 10 == 0 then AppController.pollState() end
   end

   local riverEngine = _G.river or _G.waterfall
   local stepDt = dt or (1.0 / 60.0)
   if stepDt > 0.2 then stepDt = 0.2 end
   spectrumTimer = spectrumTimer + stepDt

   while spectrumTimer >= spectrumInterval do
      spectrumTimer = spectrumTimer - spectrumInterval
      if Hardware.isHardwareEnabled() then
         local hwSpec = Hardware.getSpectrum()
         if hwSpec and #hwSpec > 0 then 
            _G.lastSpectrumData = hwSpec
            if riverEngine and riverEngine.addRow then
               riverEngine.addRow(_G.lastSpectrumData)
            end
         end
      else
         local dummy = {}
         local noiseFloor = -105
         for i = 1, 1024 do
            dummy[i] = noiseFloor + 8 * math.sin(i / 60 + _G.frameCount / 25) + 4 * math.random()
         end

         -- Inject simulated spectral peaks matching decoder tracks and active VFO
         local hwCenter = (Model.spectrumCenterFreq and Model.spectrumCenterFreq:peek()) or (riverViewRef and riverViewRef.hwCenterFreq) or 14.2e6
         local hwBw = _G.sampleRate or (riverViewRef and riverViewRef.hwBandwidth) or 384e3
         local hwMin = hwCenter - hwBw / 2

         local function addPeak(freq, strengthDb, widthBins)
            local bin = 1 + math.floor(((freq - hwMin) / hwBw) * 1024)
            for off = -widthBins, widthBins do
               local b = bin + off
               if b >= 1 and b <= 1024 then
                  local attenuation = (off * off) * (18.0 / (widthBins * widthBins))
                  local p = strengthDb - attenuation
                  if p > dummy[b] then dummy[b] = p end
               end
            end
         end

         -- FT8 signals at 14.074 MHz (distinct sharp tones)
         addPeak(14.074e6, -55, 3)
         addPeak(14.074350e6, -60, 2)
         addPeak(14.075e6, -68, 3)

         -- CW signal at 14.025 MHz
         addPeak(14.025e6, -48, 2)

         -- SSB phone signal at 14.200 and 14.225 MHz
         addPeak(14.200e6, -46, 6)
         addPeak(14.225e6, -62, 8)

         -- AM beacon on 14.250 MHz
         addPeak(14.250e6, -50, 8)

         -- WWV 15 MHz AM signal
         addPeak(15.000e6, -45, 10)

         -- Tuned VFO carrier peak
         local vfo = Model.rx.VFO.activeValue:get() or 14.240e6
         addPeak(vfo, -50, 4)

         _G.lastSpectrumData = dummy
         if riverEngine and riverEngine.addRow then
            riverEngine.addRow(_G.lastSpectrumData)
         end
      end
   end
end
return UI
