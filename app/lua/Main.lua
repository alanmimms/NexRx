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

  frameInput.mouseX = 0
  frameInput.mouseY = 0
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
    events.dispatch(eventData, rxTree)
  end
end
