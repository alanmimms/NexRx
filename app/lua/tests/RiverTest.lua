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
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, y = 2 })
  assert_eq("topHandle", box.dragMode, "top chevron handle engaged")
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
  Tests.test_frequency_ruler_events()
  Tests.test_smeter_dual_decay()
  Tests.test_gutter_left_off_screen_piles()
  Tests.test_signalbox_river_geometry()

  print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
  return failed == 0
end

return Tests
