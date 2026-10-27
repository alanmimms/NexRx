--[[
    Test for Generalized Slider and VFO Step logic
]]

local events = require("Events")
local setbox = require("SetBox")
local Model = require("Model")
local Widget = require("ui.Widget")

local passed = 0
local failed = 0

local function assert_eq(expected, actual, msg)
    local same = false
    if type(expected) == "number" and type(actual) == "number" then
        same = math.abs(expected - actual) < 0.0001
    else
        same = expected == actual
    end

    if same then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected %s, got %s)", msg, tostring(expected), tostring(actual)))
        return false
    end
end

local function registerTestHandlers()
    events.registerHandler("slider_adjust", function(event, widget, props)
        local stepFraction = props.stepFraction or 0.01
        local minVal = widget.props.min or -60
        local maxVal = widget.props.max or 0
        local current = widget.props.value or -20
        local step = (maxVal - minVal) * stepFraction
        local newVal = math.max(minVal, math.min(maxVal, current + (event.delta or 0) * step))
        widget.props.value = newVal
        _G.lastVol = newVal
        return true
    end)

    events.registerHandler("vfo_control", function(event, widget, props)
        local prop = props.property or "rx.VFO.activeValue"
        local step = props.step or 100
        local current = Model.vfo or 14.2e6
        local delta = 0
        if event.type == events.Type.MOUSE_WHEEL then
            delta = (event.delta or 0) * step
        end
        if delta ~= 0 then
            Model.vfo = current + delta
            return true
        end
        return false
    end)
end

local function test_slider_generalization()
    print("test_slider_generalization")
    setbox.clear()
    events.init()
    registerTestHandlers()

    setbox.rule {
        id = "slider-step-default",
        tags = {"widget.Slider"},
        priority = 1,
        apply = { stepFraction = 0.01 }
    }
    setbox.rule {
        id = "slider-step-shift",
        tags = {"widget.Slider", "input.SHIFT"},
        priority = 10,
        apply = { stepFraction = 0.1 }
    }
    setbox.rule {
        id = "event-slider-wheel",
        tags = {"event.MouseWheel", "widget.Slider"},
        apply = { handler = "slider_adjust" }
    }

    local widget = Widget({
        id = "vol-slider",
        tags = {"widget.Slider"},
        props = { property = "volume.DB", min = -60, max = 0, value = -20 }
    })

    local event = { type = events.Type.MOUSE_WHEEL, x = 5, y = 5, delta = 1, modifiers = {} }
    events.dispatch(event, widget)
    assert_eq(-19.4, _G.lastVol, "Default step (1%)")

    setbox.addTag("input.SHIFT")
    events.dispatch(event, widget)
    setbox.removeTag("input.SHIFT")
    assert_eq(-13.4, _G.lastVol, "Shift step (10%)")
end

local function test_vfo_generalization()
    print("test_vfo_generalization")
    setbox.clear()
    events.init()
    registerTestHandlers()

    setbox.rule {
        id = "event-vfo-wheel",
        tags = {"event.MouseWheel", "widget.VFOControl"},
        priority = 20,
        apply = { handler = "vfo_control" }
    }
    setbox.rule {
        id = "vfo-step-default",
        tags = {"widget.VFOControl"},
        apply = { step = 100 }
    }
    setbox.rule {
        id = "vfo-step-shift",
        tags = {"widget.VFOControl", "input.SHIFT"},
        priority = 10,
        apply = { step = 1000 }
    }

    Model.vfo = 14200000

    local widget = Widget({
        id = "vfo-display",
        tags = {"widget.VFOControl"},
    })

    local event = { type = events.Type.MOUSE_WHEEL, x = 5, y = 5, delta = 1, modifiers = {} }
    events.dispatch(event, widget)
    assert_eq(14200100, Model.vfo, "Default VFO step (100Hz)")

    setbox.addTag("input.SHIFT")
    events.dispatch(event, widget)
    setbox.removeTag("input.SHIFT")
    assert_eq(14201100, Model.vfo, "Shift VFO step (1000Hz)")
end

local M = {}
function M.runAll()
    print("\n=== Generalization Tests ===")
    passed = 0
    failed = 0
    test_slider_generalization()
    test_vfo_generalization()
    print(string.format("\nResults: %d passed, %d failed", passed, failed))
    return failed == 0
end

if arg and arg[0] and arg[0]:match("GeneralizationTest%.lua$") then
    if not M.runAll() then os.exit(1) end
end

return M
