--[[
  RiverTest.lua - Unit tests for The River UI architecture and widgets
]]

local RiverBehaviors = require("ui.RiverBehaviors")
local RiverView = require("ui.RiverView")
local FrequencyRuler = require("ui.FrequencyRuler")
local LiveTrace = require("ui.LiveTrace")
local GutterLeft = require("ui.GutterLeft")
local SMeter = require("ui.SMeter")
local SignalBox = require("ui.SignalBox")
local setbox = require("SetBox")
local Model = require("Model")

local Tests = {}
local passed = 0
local failed = 0

local function assert_eq(expected, actual, msg)
  if expected == actual then
    passed = passed + 1
    return true
  else
    failed = failed + 1
    print(string.format("  FAIL: %s (expected %s, got %s)", msg, tostring(expected), tostring(actual)))
    return false
  end
end

local function assert_true(value, msg)
  if value then
    passed = passed + 1
    return true
  else
    failed = failed + 1
    print(string.format("  FAIL: %s (expected true, got %s)", msg, tostring(value)))
    return false
  end
end

local function assert_close(expected, actual, tolerance, msg)
  tolerance = tolerance or 0.01
  if math.abs(expected - actual) <= tolerance then
    passed = passed + 1
    return true
  else
    failed = failed + 1
    print(string.format("  FAIL: %s (expected ~%s, got %s)", msg, tostring(expected), tostring(actual)))
    return false
  end
end

-- =============================================================================
-- Test Cases
-- =============================================================================

function Tests.test_river_behaviors()
  print("test_river_behaviors")
  -- Jump scroll calculation
  local newOffset = RiverBehaviors.jumpScroll.calculateJumpTarget(30.0, 20.0, 0.5)
  assert_eq(20.0, newOffset, "jump target offset")

  local zeroClamp = RiverBehaviors.jumpScroll.calculateJumpTarget(5.0, 20.0, 0.5)
  assert_eq(0.0, zeroClamp, "jump target clamps to 0")

  -- Loupe collision detection
  local tracks = { { y = 100 }, { y = 108 }, { y = 250 } }
  local loupeActive = RiverBehaviors.bubbleLoupe.shouldActivateBubble(50, 104, tracks, 1.0)
  assert_true(loupeActive, "bubble loupe activates on dense collision")

  local loupeInactive = RiverBehaviors.bubbleLoupe.shouldActivateBubble(50, 300, tracks, 1.0)
  assert_true(not loupeInactive, "bubble loupe idle on sparse track")
end

function Tests.test_river_view_normalized_coords()
  print("test_river_view_normalized_coords")
  local rv = RiverView({
    freqMin = 14.100e6,
    freqMax = 14.250e6,
    centerFreq = 14.175e6,
    bandwidth = 350e3,
  })

  local fMinNorm, fMaxNorm = rv:getNormalizedFreqRange()
  -- hwMin = 14.000e6, hwMax = 14.350e6
  -- fMinNorm = (14.100 - 14.000) / 0.350 = 100 / 350 = 0.2857
  -- fMaxNorm = (14.250 - 14.000) / 0.350 = 250 / 350 = 0.7143
  assert_close(0.2857, fMinNorm, 0.01, "fMinNorm")
  assert_close(0.7143, fMaxNorm, 0.01, "fMaxNorm")
end

function Tests.test_river_view_split_navigation()
  print("test_river_view_split_navigation")
  local rv = RiverView({
    w = 800,
    h = 450,
    timeSpan = 30.0,
    timeOffset = 0.0,
  })
  rv:layout(0, 0, 800, 450)

  -- Scrubbing into history
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 200 })
  rv:onEvent({ type = "mouseMotion", x = 600, y = 200 })
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 600, y = 200 })

  assert_true(rv.timeOffsetSec > 0, "scrubbed into history")

  -- Clicking in live pinned sliver (width 60, so x >= 740) resets timeOffset to 0
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 200 })
  assert_eq(0.0, rv.timeOffsetSec, "clicking pinned live sliver snaps to real time")
end

function Tests.test_frequency_ruler_events()
  print("test_frequency_ruler_events")
  local ruler = FrequencyRuler({
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    w = 60,
    h = 300,
  })
  ruler:layout(0, 0, 60, 300)

  -- Wheel zoom in
  ruler:onEvent({ type = "mouseWheel", delta = 1 })
  local newSpan = ruler.props.freqMax - ruler.props.freqMin
  assert_true(newSpan < 350e3, "wheel zooms frequency in")
end

function Tests.test_smeter_dual_decay()
  print("test_smeter_dual_decay")
  local meter = SMeter({ id = "test-smeter", w = 160, h = 34 })
  meter:layout(0, 0, 160, 34)

  -- Mock readings: jump to S9, then drop to S3
  meter:drawSelf({ sUnits = 9.0, sText = "S9", dBmText = "-73 dBm" })
  assert_eq(9.0, meter.peakUnits, "peak captured instantaneous S9")

  meter:drawSelf({ sUnits = 3.0, sText = "S3", dBmText = "-109 dBm" })
  assert_true(meter.peakUnits > 3.0, "peak slowly decays above instantaneous reading")
end

function Tests.test_gutter_left_off_screen_piles()
  print("test_gutter_left_off_screen_piles")
  local gutter = GutterLeft({
    freqMin = 14.1e6,
    freqMax = 14.2e6,
    boxes = {
      { carrierFreq = 14.3e6 }, -- Above max (top pile)
      { carrierFreq = 14.0e6 }, -- Below min (bottom pile)
      { carrierFreq = 14.15e6 } -- Visible inside span
    },
    w = 68,
    h = 300,
  })
  gutter:layout(0, 0, 68, 300)

  local clickedTop = false
  gutter.onTopPileClick = function() clickedTop = true end
  gutter:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, y = 10 })
  assert_true(clickedTop, "clicked top off-screen pile")
end

function Tests.test_gutter_left_tab_click_selection()
  print("test_gutter_left_tab_click_selection")
  local gutter = GutterLeft({
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    boxes = {
      { boxIndex = 1, carrierFreq = 14.050e6, mode = "CW", bandwidth = 500 },
      { boxIndex = 2, carrierFreq = 14.250e6, mode = "USB", bandwidth = 2800 }
    },
    w = 68,
    h = 350,
  })
  gutter:layout(0, 0, 68, 350)
  gutter:drawSelf()

  local selectedIdx = nil
  gutter.onSelectBox = function(self, idx) selectedIdx = idx end

  assert_true(#gutter.tabHitBoxes >= 2, "gutter recorded tab hit boxes")
  local tab2 = gutter.tabHitBoxes[2]
  assert_true(tab2 ~= nil, "tab 2 hit box exists")

  gutter:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, y = tab2.y + 5 })
  assert_eq(2, selectedIdx, "clicking tab in GutterLeft triggers onSelectBox with index 2")
  assert_eq(2, Model.selectedSignalBoxIndex:get(), "clicking tab sets Model.selectedSignalBoxIndex")
end

function Tests.test_signalbox_river_geometry()
  print("test_signalbox_river_geometry")
  local box = SignalBox({
    carrierFreq = 14.240e6,
    bandwidth = 2800,
    mode = "USB",
    label = "CQ W1AW",
    w = 800,
    h = 40,
  })
  box:layout(0, 100, 800, 40)

  assert_eq("USB", box.mode, "box mode")
  assert_eq(2800, box.bandwidth, "box bandwidth")

  -- Dragging top chevron handle adjusts bandwidth
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 4 })
  assert_eq("topHandle", box.dragMode, "top chevron handle engaged")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 4 })

  -- Clicking across the body of the box engages "move", NOT chevron handles
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 36 })
  assert_eq("move", box.dragMode, "clicking body of the box engages move mode")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 400, y = 36 })

  -- Dragging bottom chevron handle uses fine vernier scaling (25 Hz/pixel)
  box.mode = "USB"
  box.bandwidth = 2800
  box.carrierFreq = 14.240e6
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 36 })
  assert_eq("bottomHandle", box.dragMode, "bottom chevron handle engaged")
  box:onEvent({ type = "mouseMotion", x = 760, y = 46 }) -- moved down 10px
  assert_eq(3050, box.bandwidth, "vernier scaling: 10px moves bandwidth by 250 Hz to 3050 Hz")
  assert_close(14.2428e6 - 3050, box.carrierFreq, 1.0, "carrier lowered by 250 Hz while upper skirt anchored")
  box:onEvent({ type = "mouseMotion", x = 760, y = 36 }) -- moved back up 10px
  assert_eq(2800, box.bandwidth, "moving cursor back restores exact initial bandwidth")
  assert_close(14.240e6, box.carrierFreq, 1.0, "moving cursor back restores exact carrier frequency")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 36 })

  -- Clicking mode badge cycles mode
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 20, y = 20 })
  assert_eq("LSB", box.mode, "clicking mode badge cycled USB to LSB")
end

function Tests.test_decoder_track_lod()
  print("test_decoder_track_lod")
  local DecoderTrack = require("ui.DecoderTrack")

  -- 1. Base fallback color
  local trBase = DecoderTrack({
    freq = 14.074e6,
    mode = "UNKNOWN",
  })
  local baseCol = trBase:getRibbonColor()
  assert_true(baseCol ~= nil, "base ribbon color present")

  -- 2. Tag-driven customization by mode
  setbox.rule {
    id = "user-custom-ft8-ribbon",
    tags = { "widget.DecoderTrack", "mode.FT8" },
    priority = 200,
    apply = {
      ribbonColor = "#ffaa00",
      textColor = "#ffffff",
    }
  }

  local tr = DecoderTrack({
    freq = 14.074e6,
    mode = "FT8",
    callsign = "W1AW",
    text = "CQ W1AW FN31"
  })

  local col = tr:getRibbonColor()
  assert_close(1.0, col[1], 0.01, "FT8 custom tag color red channel")
  assert_close(0.667, col[2], 0.01, "FT8 custom tag color green channel")

  -- Render inline without error
  tr:renderInline(100, 800, 30.0, 20.0, false, 0, 0)
  assert_true(true, "rendered inline track")
end

function Tests.test_river_view_tab_cycling_and_spawning()
  print("test_river_view_tab_cycling_and_spawning")
  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    w = 800,
    h = 450,
    signalBoxes = {
      { index = 1, carrierFreq = 14.025e6, mode = "CW" },
      { index = 2, carrierFreq = 14.225e6, mode = "USB" }
    }
  })
  rv:layout(0, 0, 800, 450)

  local Model = require("Model")
  Model.selectedSignalBoxIndex:set(1)

  -- TAB cycles to box 2
  rv:onEvent({ type = "key", key = "TAB", isDown = true })
  assert_eq(2, Model.selectedSignalBoxIndex:get(), "tab cycled to box 2")

  -- Shift+TAB cycles backwards to box 1
  _G.isShiftDown = function() return true end
  rv:onEvent({ type = "key", key = "TAB", isDown = true })
  _G.isShiftDown = function() return false end
  assert_eq(1, Model.selectedSignalBoxIndex:get(), "shift+tab cycled back to box 1")

  -- TAB wraps back to box 2
  rv:onEvent({ type = "key", key = "TAB", isDown = true })
  assert_eq(2, Model.selectedSignalBoxIndex:get(), "tab cycled back to box 2")

  -- Shift+Click drops a 3rd SignalBox
  _G.isShiftDown = function() return true end
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 200 })
  _G.isShiftDown = function() return false end
  assert_eq(3, #rv.signalBoxes, "shift+click spawned 3rd signal box")
  assert_eq(3, Model.selectedSignalBoxIndex:get(), "new box 3 is selected")

  -- DELETE key removes focused box 3
  rv:onEvent({ type = "key", key = "DELETE", isDown = true })
  assert_eq(2, #rv.signalBoxes, "delete removed focused 3rd box")
  assert_eq(2, Model.selectedSignalBoxIndex:get(), "focus fell back to box 2")

  -- Direct click to tune on the River
  local targetFreq = rv:getFreqAtY(150)
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 150 })
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 400, y = 150 })
  assert_close(targetFreq, rv.signalBoxes[2].carrierFreq, 1.0, "direct click tunes focused box to target freq")
  assert_close(targetFreq, Model.rx.VFO.activeValue:get(), 1.0, "direct click updates VFO active value")
end

function Tests.test_river_view_cursor_centered_zoom()
  print("test_river_view_cursor_centered_zoom")
  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    centerFreq = 14.175e6,
    bandwidth = 500e3,
    w = 800,
    h = 450,
    signalBoxes = {
      { index = 1, carrierFreq = 14.074e6, bandwidth = 25000, mode = "USB" }
    }
  })
  rv:layout(0, 0, 800, 450)
  local initialBoxH = rv.signalBoxes[1].props.h

  -- Find y corresponding to 14.074 MHz
  local rulerH = rv.props.rulerH or 22
  local canvasH = 450 - rulerH
  local normF = (14.074e6 - 14.0e6) / 350e3
  local cursorY = rulerH + (1.0 - normF) * canvasH

  -- Wheel zoom in with cursor at 14.074 MHz
  rv:onEvent({ type = "mouseWheel", delta = 1, y = cursorY })

  local newSpan = rv.freqMax - rv.freqMin
  assert_true(newSpan < 350e3, "wheel zooms span in")
  local freqAtCursor = rv:getFreqAtY(cursorY)
  assert_close(14.074e6, freqAtCursor, 1000.0, "cursor frequency preserved after zoom")

  -- SignalBox height should have expanded proportionally as we zoomed in
  local zoomedBoxH = rv.signalBoxes[1].props.h
  assert_true(zoomedBoxH > initialBoxH, "signal box height expands on zoom in")

  -- Normalized coordinates for LiveTrace/River shrink within [0.0, 1.0]
  local fMinNorm, fMaxNorm = rv:getNormalizedFreqRange()
  assert_true(fMinNorm >= 0.0 and fMaxNorm <= 1.0, "normalized coordinates within [0, 1]")
  assert_true((fMaxNorm - fMinNorm) < (350e3 / 500e3), "normalized span shrinks on zoom in")
end

function Tests.test_spacer_non_interactive()
  print("test_spacer_non_interactive")
  local Widget = require("ui.Widget")
  local Stick = require("ui.Stick")

  local spacer = Widget.Spacer{
    name = "top-bar-spacer",
    metrics = { stick = Stick.L, flexW = 1 }
  }
  spacer:layout(0, 0, 200, 38)

  assert_true(spacer.interactive == false, "spacer has interactive=false")
  assert_true(spacer:contains(50, 19), "spacer geometry contains point")
  assert_true(spacer:hitTest(50, 19) == nil, "spacer hitTest returns nil")

  local root = Widget.Row{
    name = "bar",
    interactive = false,
    kids = {
      spacer
    }
  }
  root:layout(0, 0, 200, 38)
  local hit = Widget.updateGlobalMouse(root, 50, 19)
  assert_true(hit == nil, "updateGlobalMouse does not hover spacer")
  assert_true(spacer.isMouseOver ~= true, "spacer is not marked as mouse over")
end

-- =============================================================================
-- Test Suite Runner
-- =============================================================================

function Tests.runAll()
  print("\n=== River Module Tests ===\n")
  passed = 0
  failed = 0

  Tests.test_river_behaviors()
  Tests.test_river_view_normalized_coords()
  Tests.test_river_view_split_navigation()
  Tests.test_river_view_cursor_centered_zoom()
  Tests.test_frequency_ruler_events()
  Tests.test_smeter_dual_decay()
  Tests.test_gutter_left_off_screen_piles()
  Tests.test_gutter_left_tab_click_selection()
  Tests.test_signalbox_river_geometry()
  Tests.test_decoder_track_lod()
  Tests.test_river_view_tab_cycling_and_spawning()
  Tests.test_spacer_non_interactive()

  print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
  return failed == 0
end

return Tests
