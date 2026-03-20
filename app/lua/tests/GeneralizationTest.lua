--[[
    Test for Generalized Slider and VFO Step logic
]]

local events = require("Events")
local setbox = require("SetBox")
local Model = require("Model")
local state = require("ui.State")

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

-- We don't want to call events.init() because it clears the handlers registered in Events.lua
local function init_test_events()
    events.widgets = {}
    -- Keep events.handlers as they are registered in Events.lua
    events.modeTags = {}
    events._layoutParentStack = {}
    events._currentLayoutParent = nil
    events.nextZIndex = 1
end

local function test_slider_generalization()
    print("test_slider_generalization")
    init_test_events()
    setbox._clear()

    -- 1. Setup rules for generic sliders
    rule {
        id = "slider-step-default",
        tags = {"widget.Slider"},
        priority = 1,
        apply = { stepFraction = 0.01 }
    }
    rule {
        id = "slider-step-shift",
        tags = {"widget.Slider", "input.SHIFT"},
        priority = 10,
        apply = { stepFraction = 0.1 }
    }
    rule {
        id = "event-slider-wheel",
        tags = {"event.MouseWheel", "widget.Slider"},
        apply = { handler = "slider_adjust" }
    }

    -- 2. Mock model
    _G.lastVol = nil
    Model.volume = {
        DB = {
            set = function(_, v) _G.lastVol = v end,
            get = function() return -20 end
        }
    }
    
    -- 3. Mock widget
    local widget = {
        id = "vol-slider",
        tags = {"widget.Slider"},
        data = { property = "volume.DB", min = -60, max = 0, value = -20 }
    }
    events.widgets["vol-slider"] = { bounds = {x=0,y=0,w=100,h=20}, tags = widget.tags, data = widget.data, zIndex = 1 }

    -- 4. Test wheel WITHOUT modifiers (1% of 60 = 0.6)
    local event = { type = events.Type.MOUSE_WHEEL, x = 5, y = 5, delta = 1, modifiers = {} }
    events.dispatch(event)
    assert_eq(-19.4, _G.lastVol, "Default step (1%)")
    events.widgets["vol-slider"].data.value = _G.lastVol -- Update mock widget

    -- 5. Test wheel WITH shift (10% of 60 = 6.0)
    event.modifiers = {"input.SHIFT"}
    events.dispatch(event)
    assert_eq(-13.4, _G.lastVol, "Shift step (10%)")
end

local function test_vfo_generalization()
    print("test_vfo_generalization")
    init_test_events()
    setbox._clear()

    -- 1. Setup rules for VFO
    rule {
        id = "event-vfo-wheel",
        tags = {"event.MouseWheel", "widget.VFOControl"},
        priority = 20,
        apply = { handler = "vfo_control" }
    }
    rule {
        id = "vfo-step-default",
        tags = {"widget.VFOControl"},
        apply = { step = 100 }
    }
    rule {
        id = "vfo-step-shift",
        tags = {"widget.VFOControl", "input.SHIFT"},
        priority = 10,
        apply = { step = 1000 }
    }

    -- 2. Mock model
    Model.vfo = 14200000
    Model.set = function(prop, val) Model.vfo = val end
    Model.getSelectedSignalBox = function() return { frequency = Model.vfo } end

    -- 3. Mock widget
    local widget = {
        id = "vfo-display",
        tags = {"widget.VFOControl"},
    }
    events.widgets["vfo-display"] = { bounds = {x=0,y=0,w=100,h=40}, tags = widget.tags, zIndex = 1 }

    -- 4. Test wheel WITHOUT modifiers (100 Hz)
    local event = { type = events.Type.MOUSE_WHEEL, x = 5, y = 5, delta = 1, modifiers = {} }
    events.dispatch(event)
    assert_eq(14200100, Model.vfo, "Default VFO step (100Hz)")

    -- 5. Test wheel WITH shift (1000 Hz)
    event.modifiers = {"input.SHIFT"}
    events.dispatch(event)
    assert_eq(14201100, Model.vfo, "Shift VFO step (1000Hz)")
end

-- Mocking AppController.registerHandlers for vfo_control
events.handlers["vfo_control"] = function(event, widget, props)
    local prop = props.property or "rx.VFO.activeValue"
    local step = props.step or 100
    local current = 0
    if prop == "rx.VFO.activeValue" then
        local box = Model.getSelectedSignalBox()
        current = box and box.frequency or 14.2e6
    else
        current = 14200000 -- dummy
    end
    
    local delta = 0
    if event.type == events.Type.MOUSE_WHEEL then
        delta = event.delta * step
    end
    
    if delta ~= 0 then
        Model.set(prop, current + delta)
        return true
    end
    return false
end

print("=== Generalization Tests ===")
test_slider_generalization()
test_vfo_generalization()

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
