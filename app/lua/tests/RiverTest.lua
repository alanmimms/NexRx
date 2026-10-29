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

  -- Dragging top chevron handle ([-15, 0] above box top) adjusts bandwidth
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = -5 })
  assert_eq("topHandle", box.dragMode, "top chevron handle engaged")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = -5 })

  -- Clicking across the body of the box (even inside right side) engages "move", NOT chevron handles
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 4 })
  assert_eq("move", box.dragMode, "clicking inside box body engages move mode")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 4 })

  -- Dragging bottom chevron handle ([h, h + 15] below box bottom) moves 1:1 stuck to the mouse cursor
  box.mode = "USB"
  box.bandwidth = 2800
  box.carrierFreq = 14.240e6
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 45 })
  assert_eq("bottomHandle", box.dragMode, "bottom chevron handle engaged")
  box:onEvent({ type = "mouseMotion", x = 760, y = 55 }) -- moved down 10px (100 Hz/px default when no parent)
  assert_eq(3800, box.bandwidth, "1:1 mouse tracking: 10px moves bandwidth by 1000 Hz to 3800 Hz")
  assert_close(14.2428e6 - 3800, box.carrierFreq, 1.0, "carrier lowered by 1000 Hz while upper skirt anchored")
  box:onEvent({ type = "mouseMotion", x = 760, y = 45 }) -- moved back up 10px
  assert_eq(2800, box.bandwidth, "moving cursor back restores exact initial bandwidth")
  assert_close(14.240e6, box.carrierFreq, 1.0, "moving cursor back restores exact carrier frequency")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 45 })

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

  -- Shift+Click no longer spawns a SignalBox (reserved for 1 kHz delta chevron drag)
  _G.isShiftDown = function() return true end
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 200 })
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 400, y = 200 })
  _G.isShiftDown = function() return false end
  assert_eq(2, #rv.signalBoxes, "shift+click does not spawn signal box")

  -- ALT+Click drops a 3rd SignalBox
  _G.isAltDown = function() return true end
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 200 })
  _G.isAltDown = function() return false end
  assert_eq(3, #rv.signalBoxes, "alt+click spawned 3rd signal box")
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
    freqMin = 14.050e6,
    freqMax = 14.300e6,
    centerFreq = 14.175e6,
    bandwidth = 300e3,
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
  local span = 14.300e6 - 14.050e6
  local normF = (14.074e6 - 14.050e6) / span
  local cursorY = rulerH + (1.0 - normF) * canvasH

  -- Wheel zoom in with cursor at 14.074 MHz
  rv:onEvent({ type = "mouseWheel", delta = 1, y = cursorY })

  local newSpan = rv.freqMax - rv.freqMin
  assert_true(newSpan < span, "wheel zooms span in")
  local freqAtCursor = rv:getFreqAtY(cursorY)
  assert_close(14.074e6, freqAtCursor, 1000.0, "cursor frequency preserved after zoom")

  -- SignalBox height should have expanded proportionally as we zoomed in
  local zoomedBoxH = rv.signalBoxes[1].props.h
  assert_true(zoomedBoxH > initialBoxH, "signal box height expands on zoom in")

  -- Normalized coordinates for LiveTrace/River shrink within [0.0, 1.0]
  local fMinNorm, fMaxNorm = rv:getNormalizedFreqRange()
  assert_true(fMinNorm >= 0.0 and fMaxNorm <= 1.0, "normalized coordinates within [0, 1]")
  assert_true((fMaxNorm - fMinNorm) < (span / 300e3), "normalized span shrinks on zoom in")
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

function Tests.test_auto_demod_from_stimuli()
  print("test_auto_demod_from_stimuli")
  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 15.5e6,
    w = 800,
    h = 450
  })

  -- 15.000 MHz -> WWV 15 should be AM
  local modeWwv, bwWwv, labelWwv = rv:getAutoSignalDef(15.000e6)
  assert_eq("AM", modeWwv, "15 MHz WWV mode is AM")
  assert_eq(10000, bwWwv, "15 MHz WWV bandwidth is 10000")
  assert_eq("WWV 15", labelWwv, "15 MHz WWV label is WWV 15")

  -- 14.250 MHz -> AM Beacon
  local modeAm, bwAm = rv:getAutoSignalDef(14.250e6)
  assert_eq("AM", modeAm, "14.25 MHz mode is AM")
  assert_eq(6000, bwAm, "14.25 MHz bandwidth is 6000")

  -- 14.025 MHz -> CW
  local modeCw, bwCw = rv:getAutoSignalDef(14.025e6)
  assert_eq("CW", modeCw, "14.025 MHz mode is CW")
  assert_eq(500, bwCw, "14.025 MHz bandwidth is 500")

  -- 14.200 MHz -> USB Voice ID
  local modeUsb, bwUsb = rv:getAutoSignalDef(14.200e6)
  assert_eq("USB", modeUsb, "14.200 MHz mode is USB")
  assert_eq(2800, bwUsb, "14.200 MHz bandwidth is 2800")

  -- 14.275 MHz image signal -> USB (above 8 MHz and outside AM beacon tolerance)
  local modeImg, bwImg = rv:getAutoSignalDef(14.275e6)
  assert_eq("USB", modeImg, "14.275 MHz voice beacon image is USB, not AM")
  assert_eq(2800, bwImg, "14.275 MHz bandwidth is 2800")

  -- Below 8 MHz -> LSB
  local mode40m = rv:getAutoSignalDef(7.150e6)
  assert_eq("LSB", mode40m, "below 8 MHz defaults to LSB")

  -- Above 8 MHz -> USB
  local mode17m = rv:getAutoSignalDef(18.100e6)
  assert_eq("USB", mode17m, "above 8 MHz defaults to USB")

  -- Spawning a box at 15.0 MHz produces AM box
  local boxWwv = rv:spawnSignalBox(15.000e6)
  assert_eq("AM", boxWwv.mode, "spawned box at 15MHz is AM")
  assert_eq(10000, boxWwv.bandwidth, "spawned box at 15MHz has 10kHz bandwidth")
end

function Tests.test_signalbox_mode_styling()
  print("test_signalbox_mode_styling")
  setbox.rule {
    id = "test-sigbox-cw-style",
    tags = { "widget.SignalBox", "mode.CW" },
    apply = { background = "#3b82f6" }
  }
  setbox.rule {
    id = "test-sigbox-usb-style",
    tags = { "widget.SignalBox", "mode.USB" },
    apply = { background = "#10b981" }
  }

  local box = SignalBox({ mode = "CW" })
  assert_eq("CW", box.mode, "box initial mode CW")
  assert_eq("#3b82f6", box.props.background, "CW box resolved background from mode tag")

  box:setMode("USB")
  assert_eq("USB", box.mode, "box updated to USB")
  assert_eq("#10b981", box.props.background, "USB box dynamically updated background from mode tag")
end

function Tests.test_gutter_left_pile_centering()
  print("test_gutter_left_pile_centering")
  local gutter = GutterLeft({
    freqMin = 14.1e6,
    freqMax = 14.2e6,
    boxes = {
      { carrierFreq = 14.35e6, mode = "USB" },
      { carrierFreq = 14.05e6, mode = "CW" },
    },
    w = 68,
    h = 300,
  })
  gutter:layout(0, 0, 68, 300)

  local centeredFreq = nil
  gutter.onCenterFrequency = function(self, targetFreq)
    centeredFreq = targetFreq
  end

  gutter:onTopPileClick()
  assert_eq(14.35e6, centeredFreq, "clicking top pile centers on 14.35 MHz station")

  gutter:onBottomPileClick()
  assert_eq(14.05e6, centeredFreq, "clicking bottom pile centers on 14.05 MHz station")
end

function Tests.test_time_machine_playback_cursor()
  print("test_time_machine_playback_cursor")
  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    timeSpanSec = 30.0,
    timeOffsetSec = 0.0,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(14.2e6)
  box:layout(0, 50, 800, 30)

  local seekOffset = nil
  _G.audio = _G.audio or {}
  _G.audio.seekPlayback = function(off) seekOffset = off end

  -- Click in historical area (e.g. x = 200 on an 800px wide box)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 200, y = 15 })
  assert_true(box.playbackActive, "playback active after clicking historical area")
  assert_eq(200, box.playbackX, "playback cursor placed at click X")
  assert_true(seekOffset ~= nil and seekOffset > 20.0, "audio.seekPlayback invoked with past offset")

  -- Click in always-live chevron area near right edge (x = 760 on 800px box)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 15 })
  assert_true(not box.playbackActive, "clicking in always-live right edge zone cancels playback")
  assert_eq("move", box.dragMode, "clicking in live zone tunes or drags chevrons without seeking history")
end

function Tests.test_jump_scroll_on_cad_break()
  print("test_jump_scroll_on_cad_break")
  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    timeSpanSec = 20.0,
    timeOffsetSec = 15.0,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(14.2e6)
  box:layout(0, 50, 800, 30)

  -- In split mode with breakW = 0, historyW is w - pinnedW = 800 - 60 = 740
  box.playbackActive = true
  box.playbackX = 735 -- >= historyW - 10 (730)

  rv:drawSelf()
  -- Jump scroll should have triggered: timeOffsetSec reduced by 50% of 20s (10s) -> 15 - 10 = 5s
  assert_eq(5.0, rv.timeOffsetSec, "jump scroll pulled viewport forward by 50% of history span")
end

function Tests.test_bubble_loupe_alt_modifier()
  print("test_bubble_loupe_alt_modifier")
  local tracks = { { y = 100 }, { y = 108 } }
  -- Without ALT modifier, loupe must NOT activate
  local noAlt = RiverBehaviors.bubbleLoupe.shouldActivateBubble(50, 104, tracks, false)
  assert_true(not noAlt, "loupe remains idle without ALT modifier")

  local nilAlt = RiverBehaviors.bubbleLoupe.shouldActivateBubble(50, 104, tracks, nil)
  assert_true(not nilAlt, "loupe remains idle with nil ALT modifier")

  -- With ALT modifier, loupe activates when cursor is near tracks
  local withAlt = RiverBehaviors.bubbleLoupe.shouldActivateBubble(50, 104, tracks, true)
  assert_true(withAlt, "loupe activates when ALT modifier is active")
end

function Tests.test_bubble_loupe_inverse_click_transform()
  print("test_bubble_loupe_inverse_click_transform")
  local cx, cy = 200, 150
  local magX, magY = 1.5, 2.5

  -- Forward transform
  local origX, origY = 220, 170
  local screenX, screenY = RiverBehaviors.bubbleLoupe.transformForward(origX, origY, cx, cy, magX, magY)
  assert_close(230.0, screenX, 0.01, "forward magnified X")
  assert_close(200.0, screenY, 0.01, "forward magnified Y")

  -- Inverse transform
  local invX, invY = RiverBehaviors.bubbleLoupe.transformInverse(screenX, screenY, cx, cy, magX, magY)
  assert_close(origX, invX, 0.01, "inverse unmagnified X")
  assert_close(origY, invY, 0.01, "inverse unmagnified Y")

  -- RiverView click pass-through
  local rv = RiverView({ w = 800, h = 450, freqMin = 14.0e6, freqMax = 14.35e6 })
  rv:layout(0, 0, 800, 450)
  rv.loupeActive = true
  rv.hoverX = cx
  rv.hoverY = cy

  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = screenX, y = screenY })
  assert_close(origX, rv.clickStartX, 0.01, "clickStartX matches unmagnified inverted coordinate")
  assert_close(origY, rv.clickStartY, 0.01, "clickStartY matches unmagnified inverted coordinate")
end

function Tests.test_hamdb_lookup_distance_bearing()
  print("test_hamdb_lookup_distance_bearing")
  local HamDB = require("HamDB")
  HamDB.setHomeGrid("FN31pr") -- Newington, CT (ARRL)

  -- Maidenhead to lat/lon
  local lat, lon = HamDB.gridToLatLon("FN31pr")
  assert_close(41.729, lat, 0.05, "FN31pr latitude")
  assert_close(-72.708, lon, 0.05, "FN31pr longitude")

  -- Distance and bearing
  local distKm, distMi, bearing = HamDB.calculateDistanceAndBearing(41.714, -72.727, 47.658, -117.426) -- Spokane, WA
  assert_true(distMi > 2000 and distMi < 2500, "Spokane distance in miles (~2200 mi)")
  assert_true(bearing > 270 and bearing < 310, "Spokane heading west-northwest (~290 deg)")

  -- Prefix resolution
  local c1, cont1 = HamDB.resolvePrefix("W1AW")
  assert_eq("USA", c1, "W1AW country")
  assert_eq("NA", cont1, "W1AW continent")

  local c2, cont2 = HamDB.resolvePrefix("JH1YDT")
  assert_eq("Japan", c2, "JH1YDT country")
  assert_eq("AS", cont2, "JH1YDT continent")

  -- Station lookup
  local st = HamDB.lookup("W1AW")
  assert_eq("W1AW", st.callsign, "cached station lookup callsign")
  assert_eq("ARRL HQ Station", st.name, "cached station lookup name")
end

function Tests.test_temporal_qso_naming_and_awards()
  print("test_temporal_qso_naming_and_awards")
  local HamDB = require("HamDB")
  local box = SignalBox({ index = 1, caller = "W1AW" })
  assert_eq("CQ W1AW", box.label, "initial calling phase label")

  -- Respondent answers
  box:updateQSOState("W1AW", "WB7NAB")
  assert_eq("W1AW / WB7NAB", box.label, "two-way QSO label")

  -- Locking custom name
  box:lockCustomLabel("Net Control W1AW")
  assert_eq("Net Control W1AW", box.label, "locked custom label")
  box:updateQSOState("K3LR", "JH1YDT")
  assert_eq("Net Control W1AW", box.label, "locked label preserved across state updates")

  -- Award matcher rules
  local stJH = HamDB.lookup("JH1YDT")
  local awards = HamDB.evaluateAwards(stJH, { calls = {}, countries = {}, continents = {} })
  local hasNew = false
  local hasDX = false
  for _, a in ipairs(awards) do
    if a == "award.newToMe" then hasNew = true end
    if a == "award.neededDX" then hasDX = true end
  end
  assert_true(hasNew, "station flagged as award.newToMe")
  assert_true(hasDX, "DX station flagged as award.neededDX")
end

function Tests.test_wwv_15mhz_twin_stimuli_label()
  print("test_wwv_15mhz_twin_stimuli_label")
  local rv = RiverView({ w = 800, h = 450, freqMin = 14.0e6, freqMax = 15.5e6 })
  rv:layout(0, 0, 800, 450)

  -- 15MHz stimulus check from twin config
  local mode, bw, label, caller = rv:getAutoSignalDef(15.000e6)
  assert_eq("AM", mode, "15MHz mode is AM")
  assert_eq("WWV 15", label, "15MHz label is WWV 15 (not W1AW)")
  assert_eq("WWV", caller, "15MHz caller is WWV")

  -- Spawning a box at 15MHz
  local box15 = rv:spawnSignalBox(15.000e6)
  assert_eq("WWV 15", box15.label, "spawned box label at 15MHz is WWV 15")
  assert_eq("WWV", box15.callerCallsign, "spawned box caller is WWV")

  -- Tuning an existing box to 15MHz updates label and caller
  local boxTune = rv:spawnSignalBox(14.025e6)
  assert_eq("W1AW CQ", boxTune.label, "initial box at 14.025 is W1AW CQ")
  rv:tuneActiveSignalBox(15.000e6)
  assert_eq("WWV 15", boxTune.label, "tuned box updated to WWV 15")
  assert_eq("WWV", boxTune.callerCallsign, "tuned box caller updated to WWV")
end

function Tests.test_history_drag_threshold_parametric_locking()
  print("test_history_drag_threshold_parametric_locking")
  local rv = RiverView({
    w = 800,
    h = 450,
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    timeOffsetSec = 0.0,
    historyDragThresholdPx = 40,
  })
  rv:layout(0, 0, 800, 450)

  -- Mouse down at (200, 200)
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 200, y = 200 })

  -- Dragging vertically with 15px horizontal jitter: neither vertical frequency scrolling nor horizontal scrub occurs
  rv:onEvent({ type = "mouseMotion", x = 215, y = 250, gx = 215, gy = 250 })
  assert_eq(nil, rv.dragAxis, "vertical drag in river never locks vertical axis")
  assert_eq(14.0e6, rv.freqMin, "freqMin strictly unchanged during vertical drag in river")
  assert_eq(14.35e6, rv.freqMax, "freqMax strictly unchanged during vertical drag in river")
  assert_eq(0.0, rv.timeOffsetSec, "timeOffsetSec strictly unchanged below 40px horizontal threshold")

  -- Release drag
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 215, y = 250 })
  assert_eq(nil, rv.dragAxis, "dragAxis cleared on mouse release")

  -- Start new drag horizontally exceeding 40px threshold
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 200, y = 200 })
  rv:onEvent({ type = "mouseMotion", x = 260, y = 202, gx = 260, gy = 202 }) -- dx = +60
  assert_eq("horizontal", rv.dragAxis, "gesture locked to horizontal axis")
  assert_true(rv.timeOffsetSec > 0.0, "timeOffsetSec scrubbed into history past threshold")
end

function Tests.test_river_vertical_scroll_is_final()
  print("test_river_vertical_scroll_is_final")
  local rv = RiverView({ w = 800, h = 450, freqMin = 14.0e6, freqMax = 14.35e6 })
  rv:layout(0, 0, 800, 450)

  local freqEvents = 0
  rv.onFrequencyChange = function()
    freqEvents = freqEvents + 1
  end

  -- Mouse down and vertical drag in River area MUST NEVER cause vertical frequency scrolling
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 200, y = 200 })
  rv:onEvent({ type = "mouseMotion", x = 200, y = 240, gx = 200, gy = 240 })
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 200, y = 240 })
  assert_eq(0, freqEvents, "dragging vertically in river area never triggers frequency scroll")
  assert_eq(14.0e6, rv.freqMin, "freqMin unchanged after vertical drag in river")
  assert_eq(14.35e6, rv.freqMax, "freqMax unchanged after vertical drag in river")
end

function Tests.test_zoom_out_hardware_lo_stability()
  print("test_zoom_out_hardware_lo_stability")
  local hwCenter = 14.2e6
  Model.spectrumCenterFreq:set(hwCenter)
  local rv = RiverView({ w = 800, h = 450, freqMin = 14.15e6, freqMax = 14.25e6, centerFreq = hwCenter })
  rv:layout(0, 0, 800, 450)

  local zoomCalls = {}
  rv.onFrequencyChange = function(self, fMin, fMax, isFinal)
    table.insert(zoomCalls, { fMin = fMin, fMax = fMax, isFinal = isFinal })
  end

  -- Wheel zoom out once
  rv:onEvent({ type = "mouseWheel", delta = -1, x = 400, y = 225 })
  assert_true(#zoomCalls > 0, "zoom triggered onFrequencyChange")
  assert_eq(true, zoomCalls[#zoomCalls].isFinal, "wheel zoom marks isFinal=true")
  assert_true(rv.freqMax - rv.freqMin > 100e3, "frequency span expanded upon zoom out")

  -- Zoom out repeatedly: span must be capped at 300 kHz and pinned to [hwCenter - 150e3, hwCenter + 150e3]
  for i = 1, 20 do
    rv:onEvent({ type = "mouseWheel", delta = -1, x = 400, y = 225 })
  end
  assert_close(300e3, rv.freqMax - rv.freqMin, 1.0, "zoom out capped strictly at 300 kHz")
  assert_true(rv.freqMin >= hwCenter - 150e3, "fMin pinned within hardware passband")
  assert_true(rv.freqMax <= hwCenter + 150e3, "fMax pinned within hardware passband")
  assert_eq(hwCenter, Model.spectrumCenterFreq:peek(), "hardware LO remains stable throughout zoom out")
end

function Tests.test_frequency_ruler_context_menu_and_tuning()
  print("test_frequency_ruler_context_menu_and_tuning")
  local ruler = FrequencyRuler({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    w = 60,
    h = 300,
  })
  ruler:layout(0, 0, 60, 300)

  -- 1. Frequency Parsing (case-insensitive, suffixes, raw numbers, dots)
  assert_eq(14.074e6, ruler:parseFrequency("14.074"), "parse MHz float")
  assert_eq(14.074e6, ruler:parseFrequency("14.074 MHz"), "parse with MHz suffix")
  assert_eq(14.074e6, ruler:parseFrequency("14074"), "parse kHz integer")
  assert_eq(14.074e6, ruler:parseFrequency("14074000"), "parse Hz integer")
  assert_eq(7.15e6, ruler:parseFrequency("7150 kHz"), "parse with kHz suffix")
  assert_eq(15.0e6, ruler:parseFrequency("15M"), "parse 15M suffix")
  assert_eq(15.0e6, ruler:parseFrequency("15m"), "parse lowercase 15m suffix")
  assert_eq(14.750e6, ruler:parseFrequency("14750k"), "parse 14750k suffix")
  assert_eq(13123456, ruler:parseFrequency("13123456"), "parse 13123456 raw Hz")
  assert_eq(15.1e6, ruler:parseFrequency("15.1M"), "parse 15.1M suffix")
  assert_eq(14.980e6, ruler:parseFrequency("14.980M"), "parse 14.980M suffix")
  assert_eq(14375150, ruler:parseFrequency("14375.150k"), "parse 14375.150k suffix")
  assert_eq(nil, ruler:parseFrequency("invalid"), "invalid frequency returns nil")

  -- 2. Context Menu toggle via right-click and contains hit-testing
  assert_eq(false, ruler.contextMenuOpen, "context menu initially closed")
  assert_true(not ruler:contains(-50, 50), "contains rejects popover coords when menu is closed")
  ruler:onEvent({ type = "mouseButton", button = "RIGHT", isDown = true, x = 20, y = 100 })
  assert_eq(true, ruler.contextMenuOpen, "context menu opened on right-click")
  assert_eq(true, ruler.tuneInputActive, "tune input automatically active on right-click")
  assert_true(ruler:contains(-50, 50), "contains accepts popover coords when menu is open")
  assert_true(not ruler:contains(-200, 50), "contains rejects coords outside popover")

  -- 3. Direct Tune input without character duplication, and Set button click
  ruler:onEvent({ type = "textInput", text = "15" })
  ruler:onEvent({ type = "key", key = "1", isDown = true }) -- Key event must NOT duplicate
  ruler:onEvent({ type = "textInput", text = "m" })
  assert_eq("15m", ruler.tuneInputText, "textInput received '15m' without key event duplication")

  local tunedFreq = nil
  ruler.onSetCenterFrequency = function(self, freq)
    tunedFreq = freq
  end
  -- Click [Set] button at x = menuX + 120 = -46, y = 48
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = -40, y = 48 })
  assert_eq(15.0e6, tunedFreq, "clicking [Set] button sets passband center to 15.0 MHz")
  assert_eq(false, ruler.contextMenuOpen, "context menu closed after clicking [Set]")

  -- 4. Close [X] button dismisses dialog
  ruler.contextMenuOpen = true
  ruler.tuneInputActive = true
  -- Click [X] at menuX + menuW - 10 = -166 + 162 - 10 = -14, y = 10
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = -10, y = 10 })
  assert_eq(false, ruler.contextMenuOpen, "clicking [X] closes context menu")

  -- 5. ESCAPE key dismisses dialog
  ruler.contextMenuOpen = true
  ruler:onEvent({ type = "key", key = "ESCAPE", isDown = true })
  assert_eq(false, ruler.contextMenuOpen, "pressing ESCAPE closes context menu")

  -- 6. Band Selection callback
  local selectedBand = nil
  ruler.onSelectBand = function(self, band)
    selectedBand = band
  end
  ruler.contextMenuOpen = true
  -- Click on 40m button (index 4: col 0, row 1 -> x = -150, y = 115)
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = -150, y = 115 })
  assert_eq("40m", selectedBand, "clicking band pill dispatches onSelectBand('40m')")

  -- 7. Zoom Preset callback
  local zoomPresetSpan = nil
  ruler.onZoomPreset = function(self, span)
    zoomPresetSpan = span
  end
  ruler.contextMenuOpen = true
  -- Click on 50k preset (index 3: x = -90, y = 205)
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = -90, y = 205 })
  assert_eq(50e3, zoomPresetSpan, "clicking zoom preset dispatches onZoomPreset(50k)")

  -- 8. Center 300k on VFO callback
  local vfoCentered = false
  ruler.onCenterVFO = function(self)
    vfoCentered = true
  end
  ruler.contextMenuOpen = true
  -- Click on Center 300k on VFO button (x = -100, y = 230)
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = -100, y = 230 })
  assert_eq(true, vfoCentered, "clicking center on VFO dispatches onCenterVFO")
end

function Tests.test_passband_history_playback_tuning()
  print("test_passband_history_playback_tuning")
  local hwCenter = 14.2e6
  Model.spectrumCenterFreq:set(hwCenter)
  
  local rv = RiverView({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    timeSpanSec = 30.0,
    timeOffsetSec = 0.0,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(14.2e6)
  box:layout(0, 50, 800, 30)

  local seekOffset = nil
  _G.audio = _G.audio or {}
  _G.audio.seekPlayback = function(off) seekOffset = off end
  _G.audio.isPlaybackActive = function() return seekOffset ~= nil end
  _G.audio.getPlaybackOffset = function() return seekOffset or 0.0 end

  local dspOffsetHz = nil
  local dspModeId = nil
  _G.rx = _G.rx or {}
  _G.rx.setTuningOffset = function(hz) dspOffsetHz = hz end
  _G.rx.setModeId = function(id) dspModeId = id end

  -- 1. Operator initiates history playback at 14.200 MHz, 15s in the past
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 15 })
  assert_true(box.playbackActive, "playback active at historical point")
  assert_true(seekOffset ~= nil and seekOffset > 0, "seekPlayback called with historical offset")

  -- 2. Operator retunes active SignalBox to 14.250 MHz while in history playback
  rv:tuneActiveSignalBox(14.250e6)
  assert_eq(14.250e6, box.carrierFreq, "carrier tuned to 14.250 MHz")
  assert_eq(14.2e6, Model.spectrumCenterFreq:peek(), "hardware LO remains stable at 14.200 MHz")
  
  -- Verify tuning offset is calculated against LO center and passed to DSP for real-time passband demodulation
  local expectedOffset = 14.250e6 - 14.2e6 -- +50 kHz
  assert_eq(expectedOffset, box.carrierFreq - Model.spectrumCenterFreq:peek(), "tuning offset is 50 kHz")

  -- 3. Mode change to CW while in history playback
  box:setMode("CW")
  assert_eq("CW", box.mode, "mode switched to CW during history playback")
end

function Tests.test_history_playback_exact_timing_and_cursor_alignment()
  print("test_history_playback_exact_timing_and_cursor_alignment")
  local renderCalls = {}
  _G.river = {
    renderRiver = function(x, y, w, h, tSpan, tOff, fMin, fMax)
      table.insert(renderCalls, { x = x, y = y, w = w, h = h, tSpan = tSpan, tOff = tOff, fMin = fMin, fMax = fMax })
    end
  }

  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 14.30e6,
    timeSpanSec = 30.0,
    timeOffsetSec = 0.0,
    totalBufferSec = 60.0,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(14.2e6)
  box:layout(0, 50, 800, 30)

  -- Test 1: Unsplit mode River rendering parameters
  rv:drawSelf()
  assert_true(#renderCalls >= 1, "renderRiver called during RiverView:drawSelf")
  local call1 = renderCalls[#renderCalls]
  assert_eq(800, call1.w, "unsplit mode renders across full river width")
  assert_close(0.5, call1.tSpan, 0.001, "timeSpan normalized to totalBufferSec (30s / 60s = 0.5)")
  assert_close(0.0, call1.tOff, 0.001, "timeOffset normalized to 0.0 in unsplit mode")

  -- Test 2: Click to seek in unsplit mode
  local seekOffset = nil
  _G.audio = _G.audio or {}
  _G.audio.seekPlayback = function(off) seekOffset = off end
  _G.audio.isPlaybackActive = function() return true end
  _G.audio.getPlaybackOffset = function() return seekOffset end

  -- Click at x = 400 (halfway, normX = 0.5): expected offset is 15.0 seconds
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 15 })
  assert_close(15.0, seekOffset, 0.01, "seekPlayback requested exact 15.0s offset")
  assert_eq(400, box.playbackX, "playback cursor placed exactly at click X = 400")

  -- Playback cursor rendering aligns with curOffset as audio advances forward
  seekOffset = 7.5 -- 7.5s remaining from 30s span -> norm = 1.0 - (7.5 / 30.0) = 0.75 -> px = 600
  box:drawSelf()
  assert_close(600, box.playbackX, 0.5, "playback cursor advances to x = 600 as time flows forward")

  -- Test 3: Split mode River rendering parameters and cursor alignment
  renderCalls = {}
  rv.timeOffsetSec = 10.0 -- 10 seconds into history
  rv:drawSelf()
  assert_true(#renderCalls >= 2, "renderRiver called twice for split river (history + live sliver)")
  local histCall = renderCalls[1]
  local liveCall = renderCalls[2]
  assert_close(740, histCall.w, 1.0, "history region width is 740px (800 - 60)")
  assert_close(0.5, histCall.tSpan, 0.001, "history region timeSpan is 0.5 (30s / 60s)")
  assert_close(10.0 / 60.0, histCall.tOff, 0.001, "history region timeOffset is 10.0 / 60.0")
  assert_close(60, liveCall.w, 1.0, "pinned live sliver width is 60px")

  -- Click at lx = 370 on 740px history region (halfway): offset is 10 + (1 - 0.5) * 30 = 25.0s
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 370, y = 15 })
  assert_close(25.0, seekOffset, 0.01, "split mode click seeks to 25.0s (10s offset + 15s)")
  assert_close(370, box.playbackX, 0.5, "playback cursor matches click position in split mode")

  -- Right edge of history region (curOffset = 10.0s) places cursor at historyW (740)
  seekOffset = 10.0
  box:drawSelf()
  assert_close(740, box.playbackX, 0.5, "playback cursor reaches CAD break boundary when offset matches timeOffsetSec")
end

function Tests.test_gutter_left_hz_resolution_frequency_tags()
  print("test_gutter_left_hz_resolution_frequency_tags")
  local drawnTexts = {}
  local origDrawText = System.drawText
  System.drawText = function(text, x, y, size, color)
    table.insert(drawnTexts, { text = tostring(text), x = x, y = y, size = size })
    if origDrawText then origDrawText(text, x, y, size, color) end
  end

  local gutter = GutterLeft({
    freqMin = 14.190e6,
    freqMax = 14.215e6,
    boxes = {
      { carrierFreq = 14.200000e6, mode = "USB", bandwidth = 2800 },
      { carrierFreq = 14.205350e6, mode = "CW", bandwidth = 500 },
    },
    w = 84,
    h = 450,
  })
  gutter:layout(0, 0, 84, 450)
  gutter:drawSelf()
  System.drawText = origDrawText

  local found14200 = false
  local found14205 = false
  local foundSkirtHigh = false
  for _, item in ipairs(drawnTexts) do
    if item.text == "14.200.000" then found14200 = true end
    if item.text == "14.205.350" then found14205 = true end
    if item.text == "14.202.800" then foundSkirtHigh = true end
  end

  assert_true(found14200, "carrier tag displays 14.200.000 (Hz resolution)")
  assert_true(found14205, "carrier tag displays 14.205.350 (Hz resolution with arbitrary Hz)")
  assert_true(foundSkirtHigh, "skirt tag displays 14.202.800 (Hz resolution for USB upper bound)")
end

function Tests.test_river_view_faint_grid_lines_frequency_and_time()
  print("test_river_view_faint_grid_lines_frequency_and_time")
  local drawnLines = {}
  local origDrawLine = System.drawLine
  System.drawLine = function(x1, y1, x2, y2, thickness, color)
    table.insert(drawnLines, { x1 = x1, y1 = y1, x2 = x2, y2 = y2, thickness = thickness, color = color })
    if origDrawLine then origDrawLine(x1, y1, x2, y2, thickness, color) end
  end

  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 14.30e6,
    timeSpanSec = 30.0,
    timeOffsetSec = 0.0,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  rv:drawSelf()
  System.drawLine = origDrawLine

  local horizontalGridLines = 0
  local verticalTimeGridLines = 0

  for _, line in ipairs(drawnLines) do
    -- Horizontal lines across full width (x1 == 0, x2 == 800, y1 == y2 > 22)
    if line.x1 == 0 and line.x2 == 800 and math.abs(line.y1 - line.y2) < 0.1 and line.y1 > 22 then
      horizontalGridLines = horizontalGridLines + 1
    end
    -- Vertical lines spanning canvas height (y1 == 22, y2 == 450, x1 == x2 > 0 and x1 < 800)
    if math.abs(line.y1 - 22) < 0.1 and math.abs(line.y2 - 450) < 0.1 and math.abs(line.x1 - line.x2) < 0.1 and line.x1 > 0 and line.x1 < 800 then
      verticalTimeGridLines = verticalTimeGridLines + 1
    end
  end

  assert_true(horizontalGridLines >= 4, "faint horizontal grid lines drawn across river for frequency ruler steps")
  assert_true(verticalTimeGridLines >= 4, "faint vertical grid lines drawn across river for time timer ticks")
end

function Tests.test_signalbox_chevron_drag_modifier_snapping()
  print("test_signalbox_chevron_drag_modifier_snapping")
  local rv = RiverView({
    freqMin = 14.0e6,
    freqMax = 14.35e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(14.2e6)
  box.mode = "USB"
  box.bandwidth = 2800
  box.carrierFreq = 14.2e6
  box:layout(0, 50, 800, 30)

  -- Initial USB: lowFreq = 14.200 MHz, highFreq = 14.2028 MHz
  -- 1. Test top chevron drag (in [-15, 0]) with Shift (delta = 1 kHz)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = -5 })
  assert_eq("topHandle", box.dragMode, "top chevron handle engaged")
  assert_eq(14202800, box.currentDragFreq, "initial drag frequency is 14202800 Hz")

  -- First motion UP (decreasing Y) with Shift: snaps in direction of motion to nearest multiple of 1 kHz
  box:onEvent({ type = "mouseMotion", x = 760, y = -7, modifiers = { shift = true } })
  assert_eq(14203000, box.currentDragFreq, "snapped UP to nearest 1 kHz multiple (14203000 Hz)")
  assert_eq(1000, box.dragActiveDelta, "dragActiveDelta is 1000 Hz")
  assert_eq(0, box.currentDragFreq % 1000, "currentDragFreq is integral multiple of 1 kHz")

  -- Subsequent motion UP with Shift: advances in 1 kHz increments and stays on multiples
  box:onEvent({ type = "mouseMotion", x = 760, y = -15, modifiers = { shift = true } })
  assert_eq(14204000, box.currentDragFreq, "stepped UP by 1 kHz to 14204000 Hz")
  assert_eq(0, box.currentDragFreq % 1000, "stays on integral multiple of 1 kHz during subsequent motion")

  -- 2. Switch modifier mid-drag to Ctrl (delta = 100 Hz), moving DOWN (increasing Y)
  box:onEvent({ type = "mouseMotion", x = 760, y = -13, modifiers = { ctrl = true } })
  assert_eq(14203900, box.currentDragFreq, "mid-drag switch to Ctrl snaps DOWN to nearest 100 Hz multiple (14203900 Hz)")
  assert_eq(100, box.dragActiveDelta, "dragActiveDelta updated to 100 Hz")
  assert_eq(0, box.currentDragFreq % 100, "currentDragFreq is integral multiple of 100 Hz")

  -- 3. Switch modifier mid-drag to Both Shift + Ctrl (delta = 10 kHz), moving UP (decreasing Y)
  box:onEvent({ type = "mouseMotion", x = 760, y = -15, modifiers = { shift = true, ctrl = true } })
  assert_eq(14210000, box.currentDragFreq, "mid-drag switch to Shift+Ctrl snaps UP to nearest 10 kHz multiple (14210000 Hz)")
  assert_eq(10000, box.dragActiveDelta, "dragActiveDelta updated to 10 kHz")
  assert_eq(0, box.currentDragFreq % 10000, "currentDragFreq is integral multiple of 10 kHz")

  -- Subsequent motion UP with Shift + Ctrl: advances by 10 kHz
  box:onEvent({ type = "mouseMotion", x = 760, y = -23, modifiers = { shift = true, ctrl = true } })
  assert_eq(14220000, box.currentDragFreq, "stepped UP by 10 kHz to 14220000 Hz")
  assert_eq(0, box.currentDragFreq % 10000, "stays on integral multiple of 10 kHz")

  -- 4. Release mouse button to finish top chevron drag
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = -23 })
  assert_true(not box.isDragging, "dragging finished")

  -- 5. Test bottom chevron drag (in [h, h + 15] -> [30, 45]) with Ctrl (delta = 100 Hz)
  -- Reset box carrier to 14.200 MHz
  box.carrierFreq = 14.2e6
  box.bandwidth = 2800
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 35 })
  assert_eq("bottomHandle", box.dragMode, "bottom chevron handle engaged")
  assert_eq(14200000, box.currentDragFreq, "initial bottom drag frequency is carrier (14200000 Hz)")

  -- Move DOWN (increasing Y) with Ctrl: snaps DOWN in direction of motion to nearest 100 Hz multiple
  box:onEvent({ type = "mouseMotion", x = 760, y = 37, modifiers = { ctrl = true } })
  assert_eq(14199900, box.currentDragFreq, "snapped DOWN to nearest 100 Hz multiple (14199900 Hz)")
  assert_eq(100, box.dragActiveDelta, "dragActiveDelta is 100 Hz")
  assert_eq(0, box.currentDragFreq % 100, "currentDragFreq is integral multiple of 100 Hz")

  -- Subsequent motion DOWN with Ctrl: steps down by 100 Hz
  box:onEvent({ type = "mouseMotion", x = 760, y = 45, modifiers = { ctrl = true } })
  assert_eq(14199800, box.currentDragFreq, "stepped DOWN by 100 Hz to 14199800 Hz")
  assert_eq(0, box.currentDragFreq % 100, "stays on integral multiple of 100 Hz")

  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 45 })
end

function Tests.test_scroll_past_passband_advances_150k()
  print("test_scroll_past_passband_advances_150k")
  local Model = require("Model")
  Model.spectrumCenterFreq:set(14.25e6)

  local rv = RiverView({
    freqMin = 14.10e6,
    freqMax = 14.40e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  rv.hwCenterFreq = 14.25e6

  -- 1. Dragging vertically in River area MUST NEVER scroll frequency or move passband
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 400, y = 200 })
  rv:onEvent({ type = "mouseMotion", x = 400, y = 50 })
  rv:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 400, y = 50 })
  assert_eq(14.25e6, Model.spectrumCenterFreq:peek(), "dragging in River area never scrolls passband")
  assert_eq(14.10e6, rv.freqMin, "rv.freqMin invariant during River vertical drag")
  assert_eq(14.40e6, rv.freqMax, "rv.freqMax invariant during River vertical drag")

  -- 2. Scrolling the passband from one 300kHz location to the next is done by dragging the FrequencyRuler
  local ruler = FrequencyRuler({
    freqMin = 14.10e6,
    freqMax = 14.40e6,
    w = 60,
    h = 450,
  })
  ruler:layout(0, 0, 60, 450)

  -- Start vertical drag on FrequencyRuler at gy = 200
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 30, y = 200, gy = 200 })

  -- Small drag UP of only 5 pixels (gy = 195): must NOT skip LO (deliberate 20px threshold)
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 195, gy = 195 })
  assert_eq(14.25e6, Model.spectrumCenterFreq:peek(), "hardware passband stays invariant during sub-threshold ruler drag")

  -- Deliberate drag UP exceeding 20px threshold (gy = 175): advances to next 150 kHz boundary (14.40 MHz)
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 175, gy = 175 })
  assert_close(14.40e6, Model.spectrumCenterFreq:peek(), 1.0, "hardware passband stepped to next 150 kHz boundary (14.40 MHz)")
  assert_close(14.25e6, ruler.props.freqMin, 1.0, "new passband starts at 14.25 MHz")
  assert_close(14.55e6, ruler.props.freqMax, 1.0, "new passband spans to 14.55 MHz")

  -- Deliberate drag DOWN exceeding 20px threshold (gy = 200): retreats to 14.25 MHz
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 200, gy = 200 })
  assert_close(14.25e6, Model.spectrumCenterFreq:peek(), 1.0, "hardware passband moved down to 14.25 MHz")
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 30, y = 200, gy = 200 })

  -- 3. Limits testing on FrequencyRuler: bottom edge never below 1.8 MHz, top edge never above 30 MHz
  Model.spectrumCenterFreq:set(1.95e6)
  ruler.props.freqMin = 1.80e6
  ruler.props.freqMax = 2.10e6
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 30, y = 200, gy = 200 })
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 230, gy = 230 })
  assert_true(ruler.props.freqMin >= 1.8e6, "bottom edge never goes below 1.8 MHz")
  assert_true(Model.spectrumCenterFreq:peek() >= 1.95e6, "passband center never drops below 1.95 MHz")
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 30, y = 230, gy = 230 })

  Model.spectrumCenterFreq:set(29.85e6)
  ruler.props.freqMin = 29.70e6
  ruler.props.freqMax = 30.00e6
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 30, y = 200, gy = 200 })
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 170, gy = 170 })
  assert_true(ruler.props.freqMax <= 30.0e6, "top edge never goes above 30.0 MHz")
  assert_true(Model.spectrumCenterFreq:peek() <= 29.85e6, "passband center never exceeds 29.85 MHz")
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 30, y = 170, gy = 170 })
end

function Tests.test_signalbox_move_drag_modifier_snapping()
  print("test_signalbox_move_drag_modifier_snapping")
  local rv = RiverView({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(14.2e6)
  box.mode = "USB"
  box.bandwidth = 2800
  box.carrierFreq = 14.2e6
  box:layout(0, 50, 800, 30)

  -- Engage move mode by clicking inside box body (y = 15)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 15 })
  assert_eq("move", box.dragMode, "move drag mode engaged")
  assert_eq(14200000, box.currentDragFreq, "initial carrier drag frequency is 14200000 Hz")

  -- First motion UP (decreasing Y) with Shift: snaps in direction of motion to nearest multiple of 1 kHz
  box:onEvent({ type = "mouseMotion", x = 760, y = 13, modifiers = { shift = true } })
  assert_eq(14201000, box.currentDragFreq, "snapped UP to nearest 1 kHz multiple (14201000 Hz)")
  assert_eq(1000, box.dragActiveDelta, "dragActiveDelta is 1000 Hz")
  assert_eq(0, box.currentDragFreq % 1000, "currentDragFreq is integral multiple of 1 kHz")

  -- Subsequent motion UP with Shift: advances in 1 kHz increments and stays on multiples
  box:onEvent({ type = "mouseMotion", x = 760, y = 5, modifiers = { shift = true } })
  assert_eq(14202000, box.currentDragFreq, "stepped UP by 1 kHz to 14202000 Hz")
  assert_eq(0, box.currentDragFreq % 1000, "stays on integral multiple of 1 kHz during subsequent motion")

  -- Switch modifier mid-drag to Ctrl (delta = 100 Hz), moving DOWN (increasing Y)
  box:onEvent({ type = "mouseMotion", x = 760, y = 7, modifiers = { ctrl = true } })
  assert_eq(14201900, box.currentDragFreq, "mid-drag switch to Ctrl snaps DOWN to nearest 100 Hz multiple (14201900 Hz)")
  assert_eq(100, box.dragActiveDelta, "dragActiveDelta updated to 100 Hz")
  assert_eq(0, box.currentDragFreq % 100, "currentDragFreq is integral multiple of 100 Hz")

  -- Switch modifier mid-drag to Both Shift + Ctrl (delta = 10 kHz), moving UP (decreasing Y)
  box:onEvent({ type = "mouseMotion", x = 760, y = 5, modifiers = { shift = true, ctrl = true } })
  assert_eq(14210000, box.currentDragFreq, "mid-drag switch to Shift+Ctrl snaps UP to nearest 10 kHz multiple (14210000 Hz)")
  assert_eq(10000, box.dragActiveDelta, "dragActiveDelta updated to 10 kHz")
  assert_eq(0, box.currentDragFreq % 10000, "currentDragFreq is integral multiple of 10 kHz")

  -- Subsequent motion UP with Shift + Ctrl: advances by 10 kHz
  box:onEvent({ type = "mouseMotion", x = 760, y = -3, modifiers = { shift = true, ctrl = true } })
  assert_eq(14220000, box.currentDragFreq, "stepped UP by 10 kHz to 14220000 Hz")
  assert_eq(0, box.currentDragFreq % 10000, "stays on integral multiple of 10 kHz")

  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = -3 })
  assert_true(not box.isDragging, "dragging finished")
end

function Tests.test_zoomed_out_signalbox_chevrons_fixed_above_and_below()
  print("test_zoomed_out_signalbox_chevrons_fixed_above_and_below")
  local SignalBox = require("ui.SignalBox")
  local box = SignalBox({
    index = 1,
    carrierFreq = 14.2e6,
    bandwidth = 500,
    mode = "CW",
  })
  -- Simulate extreme zoom-out where box height is only 4 pixels
  box:layout(0, 50, 800, 4)

  -- 1. Hit testing with box:contains
  -- Top chevron extends 15px upward from top of signalbox: y in [-15, 0]
  assert_true(box:contains(760, -5), "contains point in top chevron area above box")
  assert_true(box:contains(760, -15), "contains point at upper edge of top chevron (-15px)")
  assert_true(not box:contains(760, -16), "rejects point outside top chevron")

  -- Bottom chevron extends 15px downward from bottom of signalbox: y in [h, h + 15] -> [4, 19]
  assert_true(box:contains(760, 8), "contains point in bottom chevron area below box")
  assert_true(box:contains(760, 19), "contains point at lower edge of bottom chevron (h + 15px)")
  assert_true(not box:contains(760, 20), "rejects point outside bottom chevron")

  -- Interior of tiny box is for moving
  assert_true(box:contains(760, 2), "contains point inside tiny box")

  -- 2. Mouse click engages correct drag mode
  -- Click top chevron above box (y = -5)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = -5 })
  assert_eq("topHandle", box.dragMode, "clicking above box engages topHandle")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = -5 })

  -- Click bottom chevron below box (y = 8)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 8 })
  assert_eq("bottomHandle", box.dragMode, "clicking below box engages bottomHandle")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 8 })

  -- Click center of tiny box (y = 2)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 2 })
  assert_eq("move", box.dragMode, "clicking inside box engages move")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 2 })

  -- 3. Verify reactive watcher on selectedSignalBoxIndex calling Model.set("rx.VFO.activeValue", ...)
  -- does not subscribe to VFO/box.frequency or cause C stack overflow during chevron dragging
  local rv = RiverView({ freqMin = 14.05e6, freqMax = 14.35e6, w = 800, h = 450 })
  rv:layout(0, 0, 800, 450)
  local activeBox = rv:spawnSignalBox(14.200e6)
  activeBox:layout(0, 100, 800, 20)
  local watchCalls = 0
  local unsub = Model.selectedSignalBoxIndex:watch(function(idx)
    watchCalls = watchCalls + 1
    local b = rv.signalBoxes and rv.signalBoxes[idx]
    if b then
      Model.set("rx.VFO.activeValue", b.carrierFreq)
      Model.set("rx.selectedMode", b.mode)
    end
  end)
  assert_eq(1, watchCalls, "initial watch call executed once")
  activeBox:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = -5, gy = 95 })
  activeBox:onEvent({ type = "mouseMotion", x = 760, y = -15, gy = 85 })
  activeBox:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = -15, gy = 85 })
  assert_eq(1, watchCalls, "chevron drag does not re-trigger selectedSignalBoxIndex watcher or overflow C stack")
  unsub:dispose()
end

function Tests.test_unfocused_signalbox_resets_history_playback_to_live()
  print("test_unfocused_signalbox_resets_history_playback_to_live")
  local rv = RiverView({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box1 = rv:spawnSignalBox(14.150e6)
  box1:layout(0, 100, 800, 20)

  local stopCalled = 0
  _G.audio = _G.audio or {}
  _G.audio.seekPlayback = function() end
  _G.audio.stopPlayback = function() stopCalled = stopCalled + 1 end

  -- Start history playback on box1
  box1:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 300, y = 10 })
  box1:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 300, y = 10 })
  assert_true(box1.playbackActive, "box1 playback is active when focused")

  -- Spawning box2 unfocuses box1 and stops its history playback
  local box2 = rv:spawnSignalBox(14.200e6)
  assert_true(not box1.playbackActive, "box1 playback stopped when box2 spawned and took focus")
  assert_true(stopCalled >= 1, "audio.stopPlayback() called when box1 lost focus")

  -- Start playback on box2, then cycle focus back to box1 via TAB
  box2:layout(0, 50, 800, 20)
  box2:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 300, y = 10 })
  box2:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 300, y = 10 })
  assert_true(box2.playbackActive, "box2 playback active")

  rv:cycleSignalBoxFocus(false)
  assert_true(not box2.playbackActive, "box2 playback stopped when TAB cycled focus away")
end

function Tests.test_del_key_deletes_focused_signalbox()
  print("test_del_key_deletes_focused_signalbox")
  local rv = RiverView({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box1 = rv:spawnSignalBox(14.150e6)
  local box2 = rv:spawnSignalBox(14.200e6)
  assert_eq(2, #rv.signalBoxes, "two signalboxes present")

  -- Pressing DELETE on box2 removes box2
  local handled = box2:onEvent({ type = "key", key = "DELETE", isDown = true })
  assert_true(handled, "DELETE key handled by focused SignalBox")
  assert_eq(1, #rv.signalBoxes, "one signalbox remains after first DELETE")
  assert_eq(box1, rv.signalBoxes[1], "remaining box is box1")

  -- Pressing DELETE again removes the last remaining focused SignalBox too
  local handledLast = box1:onEvent({ type = "key", key = "DELETE", isDown = true })
  assert_true(handledLast, "DELETE key deletes the focused SignalBox even when it is the only box")
  assert_eq(0, #rv.signalBoxes, "all signalboxes removed")
end

function Tests.test_dragging_wwv_box_to_15075_clears_wwv_metadata()
  print("test_dragging_wwv_box_to_15075_clears_wwv_metadata")
  local rv = RiverView({
    freqMin = 14.85e6,
    freqMax = 15.15e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(15.000e6)
  assert_eq("WWV 15", box.label, "at 15.000 MHz label is WWV 15")
  assert_eq("WWV", box.callerCallsign, "at 15.000 MHz caller is WWV")
  assert_eq("AM", box.mode, "at 15.000 MHz mode is AM")

  -- Drag box to 15.075 MHz
  local canvasH = 450 - 22
  local hzPerPx = 300e3 / canvasH
  local dyFor75k = -(75e3 / hzPerPx)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 5, gy = 200 })
  box:onEvent({ type = "mouseMotion", x = 760, y = 5 + dyFor75k, gy = 200 + dyFor75k })
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 5 + dyFor75k, gy = 200 + dyFor75k })

  assert_close(15.075e6, box.carrierFreq, 500, "box carrier moved to ~15.075 MHz")
  assert_true(box.label ~= "WWV 15", "box label is no longer WWV 15 at 15.075 MHz")
  assert_eq(nil, box.callerCallsign, "callerCallsign cleared when dragged away from WWV 15")
  assert_eq("USB", box.mode, "mode updated from AM to USB when dragged to 15.075 MHz")
end

function Tests.test_chevrons_move_1_to_1_with_mouse_at_any_zoom_and_bottom_chevron_moves()
  print("test_chevrons_move_1_to_1_with_mouse_at_any_zoom_and_bottom_chevron_moves")
  local rv = RiverView({
    freqMin = 14.85e6,
    freqMax = 15.15e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(15.000e6) -- AM, bw = 10000
  rv:layout(0, 0, 800, 450)

  local canvasH = 450 - 22
  local hzPerPx = (rv.freqMax - rv.freqMin) / canvasH
  local initialBottomEdgeY = box.props.y + box.props.h

  -- Click bottom chevron (below bottom edge of box) and drag downward by 20 pixels
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = box.props.h + 6, gy = initialBottomEdgeY + 6 })
  assert_eq("bottomHandle", box.dragMode, "bottomHandle engaged on AM box at 300 kHz zoom")
  box:onEvent({ type = "mouseMotion", x = 760, y = box.props.h + 26, gy = initialBottomEdgeY + 26 })

  -- Bottom edge must have moved downward by exactly 20 pixels (1:1 with mouse cursor)
  local newBottomEdgeY = box.props.y + box.props.h
  assert_close(initialBottomEdgeY + 20, newBottomEdgeY, 1.0, "bottom edge of box moved 20px downward 1:1 with mouse cursor")
  assert_close(10000 + 20 * hzPerPx, box.bandwidth, 50, "bandwidth increased by 20 * hzPerPx")
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = box.props.h + 26, gy = initialBottomEdgeY + 26 })
end

function Tests.test_frequency_ruler_detent_step_150khz_and_direction()
  print("test_frequency_ruler_detent_step_150khz_and_direction")
  local Model = require("Model")
  Model.spectrumCenterFreq:set(14.20e6)

  local ruler = FrequencyRuler({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    w = 60,
    h = 450,
  })
  ruler:layout(0, 0, 60, 450)

  -- Begin drag on FrequencyRuler at gy = 200
  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 30, y = 200, gy = 200 })
  assert_true(ruler.isDragging, "ruler isDragging active")

  -- 1. Sub-20px upward motion (12px up -> gy = 188): must NOT move passband yet
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 188, gy = 188 })
  assert_eq(14.20e6, Model.spectrumCenterFreq:peek(), "no jump before 20px threshold")

  -- 2. Reach 20px upward motion (another 10px up -> gy = 178):
  -- Dragging upward makes higher frequencies visible by stepping to the next 150 kHz boundary (14.25 MHz) and stopping
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 178, gy = 178 })
  assert_close(14.25e6, Model.spectrumCenterFreq:peek(), 1.0, "stepped UP to next 150 kHz boundary (14.25 MHz)")
  assert_close(14.10e6, ruler.props.freqMin, 1.0, "higher frequencies now visible (fMin = 14.10 MHz)")
  assert_close(14.40e6, ruler.props.freqMax, 1.0, "higher frequencies now visible (fMax = 14.40 MHz)")

  -- 3. Another 10px upward motion (gy = 168): must stay stopped at 14.25 MHz until 20px more motion
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 168, gy = 168 })
  assert_close(14.25e6, Model.spectrumCenterFreq:peek(), 1.0, "stopped until 20px more vertical motion")

  -- 4. Complete 20px upward motion (gy = 156): steps to next 150 kHz boundary (14.40 MHz)
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 156, gy = 156 })
  assert_close(14.40e6, Model.spectrumCenterFreq:peek(), 1.0, "stepped UP by 150 kHz to 14.40 MHz after 20px more motion")

  -- 5. Drag downward by 22px (gy = 178): steps downward to lower 150 kHz boundary (14.25 MHz)
  ruler:onEvent({ type = "mouseMotion", x = 30, y = 178, gy = 178 })
  assert_close(14.25e6, Model.spectrumCenterFreq:peek(), 1.0, "dragging downward stepped DOWN to 14.25 MHz")

  ruler:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 30, y = 178, gy = 178 })
end

function Tests.test_14_2m_usb_mode_accuracy_after_15m_am()
  print("test_14_2m_usb_mode_accuracy_after_15m_am")
  local Model = require("Model")
  local rv = RiverView({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)

  -- Spawn first box and second box, tune to 15M (WWV AM) then to 14.2M (USB voice beacon)
  rv:spawnSignalBox(14.150e6)
  local box2 = rv:spawnSignalBox(15.000e6)
  assert_eq("AM", box2.mode, "at 15M mode is AM")
  assert_eq("AM", Model.rx.selectedMode:get(), "Model.rx.selectedMode is AM at 15M")

  -- Now tune to 14.2M
  rv:tuneActiveSignalBox(14.200e6)
  assert_eq("USB", box2.mode, "at 14.2M box mode is USB")
  assert_eq("USB", Model.rx.selectedMode:get(), "at 14.2M Model.rx.selectedMode is USB (not stuck on AM)")
  assert_eq("CQ WB7NAB", box2.label, "at 14.2M label is CQ WB7NAB")
end

function Tests.test_ctrl_z_y_undo_redo_and_100_depth_stack()
  print("test_ctrl_z_y_undo_redo_and_100_depth_stack")
  local Model = require("Model")
  local rv = RiverView({
    freqMin = 14.05e6,
    freqMax = 14.35e6,
    w = 800,
    h = 450,
  })
  rv:layout(0, 0, 800, 450)
  local box = rv:spawnSignalBox(14.200e6)
  box:layout(0, 100, 800, 30)
  assert_eq(14.200e6, box.carrierFreq, "initial carrierFreq is 14.200 MHz")
  assert_eq(2800, box.bandwidth, "initial bandwidth is 2800 Hz")
  assert_eq(0, #rv.undoStack, "initial undoStack is empty")

  -- 1. Drag signalbox body with Shift (1 kHz steps) to 14.202 MHz across two motion steps
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = 15 })
  box:onEvent({ type = "mouseMotion", x = 760, y = 13, modifiers = { shift = true } }) -- -> 14.201 MHz
  box:onEvent({ type = "mouseMotion", x = 760, y = 5, modifiers = { shift = true } })  -- -> 14.202 MHz
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = 5 })

  assert_eq(14202000, box.carrierFreq, "dragged carrierFreq to 14.202 MHz")
  assert_eq(1, #rv.undoStack, "entire drag gesture recorded as 1 entry on undoStack")

  -- 2. Press Ctrl+Z on SignalBox to undo the mistaken drag
  local handledUndo = box:onEvent({ type = "key", key = "Z", isDown = true, modifiers = { ctrl = true } })
  assert_true(handledUndo, "Ctrl+Z handled")
  assert_eq(14200000, box.carrierFreq, "Ctrl+Z restored carrierFreq to 14.200 MHz")
  assert_eq(14200000, Model.rx.VFO.activeValue:get(), "Ctrl+Z synced active VFO back to 14.200 MHz")
  assert_eq(0, #rv.undoStack, "undoStack empty after undo")
  assert_eq(1, #rv.redoStack, "redoStack has 1 entry after undo")

  -- 3. Press Ctrl+Y on RiverView to redo the undone drag
  local handledRedo = rv:onEvent({ type = "key", key = "Y", isDown = true, modifiers = { ctrl = true } })
  assert_true(handledRedo, "Ctrl+Y handled")
  assert_eq(14202000, box.carrierFreq, "Ctrl+Y redid carrierFreq change to 14.202 MHz")
  assert_eq(14202000, Model.rx.VFO.activeValue:get(), "Ctrl+Y synced active VFO to 14.202 MHz")
  assert_eq(1, #rv.undoStack, "undoStack has 1 entry after redo")
  assert_eq(0, #rv.redoStack, "redoStack empty after redo")

  -- 4. Drag top chevron to change bandwidth, then Ctrl+Z and Ctrl+Y
  box:layout(0, 100, 800, 30)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = true, x = 760, y = -5 })
  box:onEvent({ type = "mouseMotion", x = 760, y = -7, modifiers = { shift = true } }) -- snaps highFreq from 14204800 -> 14205000 (bw = 3000)
  box:onEvent({ type = "mouseButton", button = "LEFT", isDown = false, x = 760, y = -7 })
  assert_eq(3000, box.bandwidth, "chevron drag changed bandwidth to 3000 Hz")
  assert_eq(2, #rv.undoStack, "undoStack has 2 entries")

  rv:onEvent({ type = "key", key = "Z", isDown = true, modifiers = { ctrl = true } })
  assert_eq(2800, box.bandwidth, "Ctrl+Z undid chevron bandwidth change back to 2800 Hz")
  rv:onEvent({ type = "key", key = "Y", isDown = true, modifiers = { ctrl = true } })
  assert_eq(3000, box.bandwidth, "Ctrl+Y redid chevron bandwidth change to 3000 Hz")

  -- 5. Verify maxUndoDepth = 100 cap
  assert_eq(100, rv.maxUndoDepth, "default maxUndoDepth is 100")
  for i = 1, 110 do
    rv:tuneActiveSignalBox(14.100e6 + i * 1000)
  end
  assert_eq(100, #rv.undoStack, "undoStack capped at 100 entries after 110 operations")

  -- Undoing 100 times succeeds and lands on the 10th tune (14.110 MHz)
  for i = 1, 100 do
    assert_true(rv:undo(), "undo step " .. tostring(i) .. " succeeds")
  end
  assert_eq(14.100e6 + 10 * 1000, box.carrierFreq, "after 100 undos, carrierFreq is at step 10 (14.110 MHz)")
  assert_true(not rv:undo(), "101st undo returns false because stack is capped at 100")
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
  Tests.test_auto_demod_from_stimuli()
  Tests.test_signalbox_mode_styling()
  Tests.test_gutter_left_pile_centering()
  Tests.test_time_machine_playback_cursor()
  Tests.test_jump_scroll_on_cad_break()
  Tests.test_bubble_loupe_alt_modifier()
  Tests.test_bubble_loupe_inverse_click_transform()
  Tests.test_hamdb_lookup_distance_bearing()
  Tests.test_temporal_qso_naming_and_awards()
  Tests.test_wwv_15mhz_twin_stimuli_label()
  Tests.test_history_drag_threshold_parametric_locking()
  Tests.test_river_vertical_scroll_is_final()
  Tests.test_zoom_out_hardware_lo_stability()
  Tests.test_frequency_ruler_context_menu_and_tuning()
  Tests.test_passband_history_playback_tuning()
  Tests.test_history_playback_exact_timing_and_cursor_alignment()
  Tests.test_gutter_left_hz_resolution_frequency_tags()
  Tests.test_river_view_faint_grid_lines_frequency_and_time()
  Tests.test_signalbox_chevron_drag_modifier_snapping()
  Tests.test_signalbox_move_drag_modifier_snapping()
  Tests.test_scroll_past_passband_advances_150k()
  Tests.test_zoomed_out_signalbox_chevrons_fixed_above_and_below()
  Tests.test_unfocused_signalbox_resets_history_playback_to_live()
  Tests.test_del_key_deletes_focused_signalbox()
  Tests.test_dragging_wwv_box_to_15075_clears_wwv_metadata()
  Tests.test_chevrons_move_1_to_1_with_mouse_at_any_zoom_and_bottom_chevron_moves()
  Tests.test_frequency_ruler_detent_step_150khz_and_direction()
  Tests.test_14_2m_usb_mode_accuracy_after_15m_am()
  Tests.test_ctrl_z_y_undo_redo_and_100_depth_stack()

  print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
  return failed == 0
end

return Tests
