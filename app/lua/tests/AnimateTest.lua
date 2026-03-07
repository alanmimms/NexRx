--[[
    Unit tests for animate.lua module
]]

local animate = require("Animate")

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

local function assert_close(expected, actual, tolerance, msg)
    tolerance = tolerance or 0.001
    if math.abs(expected - actual) <= tolerance then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected ~%.4f, got %.4f)", msg, expected, actual))
        return false
    end
end

local function assert_true(value, msg)
    if value then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected true)", msg))
        return false
    end
end

local function assert_nil(value, msg)
    if value == nil then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected nil, got %s)", msg, tostring(value)))
        return false
    end
end

-- =============================================================================
-- Easing Function Tests
-- =============================================================================

function Tests.test_linear_easing()
    print("test_linear_easing")
    local linear = animate.Easing.linear

    assert_close(0.0, linear(0.0), 0.001, "linear at 0")
    assert_close(0.25, linear(0.25), 0.001, "linear at 0.25")
    assert_close(0.5, linear(0.5), 0.001, "linear at 0.5")
    assert_close(0.75, linear(0.75), 0.001, "linear at 0.75")
    assert_close(1.0, linear(1.0), 0.001, "linear at 1")
end

function Tests.test_easeIn_easing()
    print("test_easeIn_easing")
    local easeIn = animate.Easing.easeIn

    assert_close(0.0, easeIn(0.0), 0.001, "easeIn at 0")
    assert_close(0.0625, easeIn(0.25), 0.001, "easeIn at 0.25")  -- 0.25^2
    assert_close(0.25, easeIn(0.5), 0.001, "easeIn at 0.5")      -- 0.5^2
    assert_close(1.0, easeIn(1.0), 0.001, "easeIn at 1")

    -- Should start slow (value < t early on)
    assert_true(easeIn(0.3) < 0.3, "easeIn slower at start")
end

function Tests.test_easeOut_easing()
    print("test_easeOut_easing")
    local easeOut = animate.Easing.easeOut

    assert_close(0.0, easeOut(0.0), 0.001, "easeOut at 0")
    assert_close(1.0, easeOut(1.0), 0.001, "easeOut at 1")

    -- Should start fast (value > t early on)
    assert_true(easeOut(0.3) > 0.3, "easeOut faster at start")
    -- Should slow at end
    assert_true(easeOut(0.9) > 0.9, "easeOut still approaching 1")
end

function Tests.test_easeInOut_easing()
    print("test_easeInOut_easing")
    local easeInOut = animate.Easing.easeInOut

    assert_close(0.0, easeInOut(0.0), 0.001, "easeInOut at 0")
    assert_close(0.5, easeInOut(0.5), 0.001, "easeInOut at 0.5")
    assert_close(1.0, easeInOut(1.0), 0.001, "easeInOut at 1")

    -- Should be slow at start
    assert_true(easeInOut(0.25) < 0.25, "easeInOut slow at start")
    -- Should be fast at end
    assert_true(easeInOut(0.75) > 0.75, "easeInOut fast at end")
end

function Tests.test_cubic_easings()
    print("test_cubic_easings")

    assert_close(0.0, animate.Easing.cubicIn(0.0), 0.001, "cubicIn at 0")
    assert_close(1.0, animate.Easing.cubicIn(1.0), 0.001, "cubicIn at 1")
    assert_close(0.125, animate.Easing.cubicIn(0.5), 0.001, "cubicIn at 0.5")  -- 0.5^3

    assert_close(0.0, animate.Easing.cubicOut(0.0), 0.001, "cubicOut at 0")
    assert_close(1.0, animate.Easing.cubicOut(1.0), 0.001, "cubicOut at 1")

    assert_close(0.0, animate.Easing.cubicInOut(0.0), 0.001, "cubicInOut at 0")
    assert_close(0.5, animate.Easing.cubicInOut(0.5), 0.001, "cubicInOut at 0.5")
    assert_close(1.0, animate.Easing.cubicInOut(1.0), 0.001, "cubicInOut at 1")
end

function Tests.test_sine_easings()
    print("test_sine_easings")

    assert_close(0.0, animate.Easing.sineIn(0.0), 0.001, "sineIn at 0")
    assert_close(1.0, animate.Easing.sineIn(1.0), 0.001, "sineIn at 1")

    assert_close(0.0, animate.Easing.sineOut(0.0), 0.001, "sineOut at 0")
    assert_close(1.0, animate.Easing.sineOut(1.0), 0.001, "sineOut at 1")

    assert_close(0.0, animate.Easing.sineInOut(0.0), 0.001, "sineInOut at 0")
    assert_close(0.5, animate.Easing.sineInOut(0.5), 0.01, "sineInOut at 0.5")
    assert_close(1.0, animate.Easing.sineInOut(1.0), 0.001, "sineInOut at 1")
end

-- =============================================================================
-- Animation Tests
-- =============================================================================

function Tests.test_simple_animation()
    print("test_simple_animation")

    local target = { value = 0 }
    animate.to(target, "value", 0, 100, 1.0, "linear")

    -- At t=0, value should be 0
    assert_close(0, target.value, 0.1, "value at t=0")

    -- Update halfway
    animate.update(0.5)
    assert_close(50, target.value, 0.1, "value at t=0.5")

    -- Update to completion
    animate.update(0.5)
    assert_close(100, target.value, 0.1, "value at t=1.0")
end

function Tests.test_animation_completion()
    print("test_animation_completion")

    local target = { x = 0 }
    local completed = false

    animate.to(target, "x", 0, 200, 0.5, "linear", function()
        completed = true
    end)

    animate.update(0.3)
    assert_true(not completed, "not completed at t=0.3")

    animate.update(0.3)  -- t=0.6, past duration
    assert_true(completed, "completed callback called")
    assert_close(200, target.x, 0.1, "final value reached")
end

function Tests.test_animation_removed_after_completion()
    print("test_animation_removed_after_completion")

    local target = { y = 0 }
    local id = animate.to(target, "y", 0, 50, 0.2, "linear")

    assert_true(animate.active[id] ~= nil, "animation active before completion")

    animate.update(0.3)  -- Past duration

    assert_nil(animate.active[id], "animation removed after completion")
end

function Tests.test_keyframe_animation()
    print("test_keyframe_animation")

    local target = { scale = 1.0 }

    animate.keyframes(target, "scale", {
        {time = 0, value = 1.0, easing = "linear"},
        {time = 0.5, value = 1.5, easing = "linear"},
        {time = 1, value = 1.0},
    }, 1.0)

    -- At t=0
    assert_close(1.0, target.scale, 0.01, "scale at t=0")

    -- At t=0.25 (halfway to first keyframe)
    animate.update(0.25)
    assert_close(1.25, target.scale, 0.01, "scale at t=0.25")

    -- At t=0.5 (first keyframe)
    animate.update(0.25)
    assert_close(1.5, target.scale, 0.01, "scale at t=0.5")

    -- At t=0.75 (halfway from peak to end)
    animate.update(0.25)
    assert_close(1.25, target.scale, 0.01, "scale at t=0.75")

    -- At t=1.0
    animate.update(0.25)
    assert_close(1.0, target.scale, 0.01, "scale at t=1.0")
end

function Tests.test_stop_animation()
    print("test_stop_animation")

    local target = { pos = 0 }
    local id = animate.to(target, "pos", 0, 100, 1.0, "linear")

    animate.update(0.3)
    assert_close(30, target.pos, 0.1, "value at t=0.3")

    -- Stop without skip to end
    animate.stop(id, false)
    assert_nil(animate.active[id], "animation stopped")
    assert_close(30, target.pos, 0.1, "value unchanged after stop")
end

function Tests.test_stop_animation_skip_to_end()
    print("test_stop_animation_skip_to_end")

    local target = { pos = 0 }
    local id = animate.to(target, "pos", 0, 100, 1.0, "linear")

    animate.update(0.2)
    assert_close(20, target.pos, 0.1, "value at t=0.2")

    -- Stop with skip to end
    animate.stop(id, true)
    assert_close(100, target.pos, 0.1, "value skipped to end")
end

function Tests.test_multiple_animations()
    print("test_multiple_animations")

    local target = {
        x = 0,
        y = 0,
        alpha = 1.0
    }

    animate.to(target, "x", 0, 100, 1.0, "linear")
    animate.to(target, "y", 0, 200, 1.0, "linear")
    animate.to(target, "alpha", 1.0, 0.0, 1.0, "linear")

    animate.update(0.5)

    assert_close(50, target.x, 0.1, "x at t=0.5")
    assert_close(100, target.y, 0.1, "y at t=0.5")
    assert_close(0.5, target.alpha, 0.01, "alpha at t=0.5")
end

function Tests.test_pause_resume()
    print("test_pause_resume")

    local target = { value = 0 }
    local id = animate.to(target, "value", 0, 100, 1.0, "linear")

    animate.update(0.3)
    assert_close(30, target.value, 0.1, "value at t=0.3")

    animate.pause(id)
    animate.update(0.3)  -- Should not progress
    assert_close(30, target.value, 0.1, "value unchanged while paused")

    animate.resume(id)
    animate.update(0.3)  -- Should progress now
    assert_close(60, target.value, 0.1, "value progressed after resume")
end

function Tests.test_easing_in_keyframes()
    print("test_easing_in_keyframes")

    local target = { v = 0 }

    animate.keyframes(target, "v", {
        {time = 0, value = 0, easing = "easeIn"},
        {time = 1, value = 100},
    }, 1.0)

    -- At t=0.5 with easeIn, value should be less than 50
    animate.update(0.5)
    assert_true(target.v < 50, "easeIn produces slower start")
    assert_true(target.v > 0, "value is progressing")
end

function Tests.test_register_custom_easing()
    print("test_register_custom_easing")

    -- Register a custom bounce easing
    animate.registerEasing("customStep", function(t)
        return t < 0.5 and 0 or 1  -- Step function
    end)

    assert_true(animate.Easing.customStep ~= nil, "custom easing registered")
    assert_eq(0, animate.Easing.customStep(0.3), "custom easing at 0.3")
    assert_eq(1, animate.Easing.customStep(0.7), "custom easing at 0.7")
end

function Tests.test_zero_duration()
    print("test_zero_duration")

    local target = { v = 0 }
    animate.to(target, "v", 0, 100, 0, "linear")

    animate.update(0.001)  -- Any update should complete it

    assert_close(100, target.v, 0.1, "instant animation completes immediately")
end

function Tests.test_interpolation_clamped()
    print("test_interpolation_clamped")

    local target = { v = 50 }
    animate.to(target, "v", 0, 100, 0.5, "linear")

    -- Update past completion
    animate.update(1.0)

    -- Value should be clamped at final value
    assert_close(100, target.v, 0.1, "value clamped at max")
end

-- =============================================================================
-- Run all tests
-- =============================================================================

function Tests.runAll()
    print("\n=== Animate Module Tests ===\n")
    passed = 0
    failed = 0

    -- Clear any existing animations before tests
    animate.active = {}

    Tests.test_linear_easing()
    Tests.test_easeIn_easing()
    Tests.test_easeOut_easing()
    Tests.test_easeInOut_easing()
    Tests.test_cubic_easings()
    Tests.test_sine_easings()

    animate.active = {}
    Tests.test_simple_animation()

    animate.active = {}
    Tests.test_animation_completion()

    animate.active = {}
    Tests.test_animation_removed_after_completion()

    animate.active = {}
    Tests.test_keyframe_animation()

    animate.active = {}
    Tests.test_stop_animation()

    animate.active = {}
    Tests.test_stop_animation_skip_to_end()

    animate.active = {}
    Tests.test_multiple_animations()

    animate.active = {}
    Tests.test_pause_resume()

    animate.active = {}
    Tests.test_easing_in_keyframes()

    Tests.test_register_custom_easing()

    animate.active = {}
    Tests.test_zero_duration()

    animate.active = {}
    Tests.test_interpolation_clamped()

    print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
    return failed == 0
end

return Tests
