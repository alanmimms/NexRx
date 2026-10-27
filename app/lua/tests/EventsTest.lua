--[[
    Unit tests for Events.lua and Widget hierarchical event handling
]]

local events = require("Events")
local Widget = require("ui.Widget")
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

local function assert_false(value, msg)
    if not value then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected false, got %s)", msg, tostring(value)))
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
-- Tests
-- =============================================================================

function Tests.test_init_and_register_handler()
    print("test_init_and_register_handler")
    events.init()

    local called = false
    events.registerHandler("test_handler", function(event, widget, props)
        called = true
        return true
    end)

    assert_true(events.getHandler("test_handler") ~= nil, "handler registered")
    events.getHandler("test_handler")({}, nil, {})
    assert_true(called, "handler was called")
end

function Tests.test_mode_tags()
    print("test_mode_tags")
    setbox.clear()
    events.addModeTag("state.FreqEntryMode")
    assert_true(events.hasModeTag("state.FreqEntryMode"), "mode tag added")
    events.removeModeTag("state.FreqEntryMode")
    assert_false(events.hasModeTag("state.FreqEntryMode"), "mode tag removed")
end

function Tests.test_create_event_and_types()
    print("test_create_event_and_types")
    assert_eq("mouseButton", events.Type.MOUSE_DOWN, "MOUSE_DOWN")
    assert_eq("mouseMotion", events.Type.MOUSE_MOVE, "MOUSE_MOVE")
    assert_eq("mouseWheel", events.Type.MOUSE_WHEEL, "MOUSE_WHEEL")
    assert_eq("key", events.Type.KEY_DOWN, "KEY_DOWN")
    assert_eq("textInput", events.Type.TEXT_INPUT, "TEXT_INPUT")

    local ev = events.createEvent(events.Type.MOUSE_DOWN, { x = 150, y = 200, button = "LEFT", isDown = true })
    assert_eq("mouseButton", ev.type, "event type")
    assert_eq(150, ev.x, "event x")
    assert_eq(200, ev.y, "event y")
    assert_true(ev.timestamp ~= nil, "event has timestamp")
end

function Tests.test_widget_hit_test_and_hover_tags()
    print("test_widget_hit_test_and_hover_tags")
    local btn1 = Widget({ id = "btn1", x = 10, y = 10, w = 100, h = 50 })
    local btn2 = Widget({ id = "btn2", x = 200, y = 10, w = 100, h = 50 })
    local root = Widget.Container({ id = "root", x = 0, y = 0, w = 400, h = 300, kids = { btn1, btn2 } })

    local hit1 = Widget.updateGlobalMouse(root, 50, 30)
    assert_eq(btn1, hit1, "hit btn1")
    assert_true(btn1:hasTag("state.Hovered"), "btn1 has state.Hovered")
    assert_false(btn2:hasTag("state.Hovered"), "btn2 does not have state.Hovered")

    local hit2 = Widget.updateGlobalMouse(root, 250, 30)
    assert_eq(btn2, hit2, "hit btn2")
    assert_false(btn1:hasTag("state.Hovered"), "btn1 lost state.Hovered")
    assert_true(btn2:hasTag("state.Hovered"), "btn2 gained state.Hovered")

    local hitRoot = Widget.updateGlobalMouse(root, 150, 30)
    assert_eq(root, hitRoot, "hit root in gap")

    local miss = Widget.updateGlobalMouse(root, 500, 500)
    assert_nil(miss, "outside root returns nil")
end

function Tests.test_hierarchical_event_bubbling()
    print("test_hierarchical_event_bubbling")
    local childSeenLocalX = nil
    local parentSeenLocalX = nil

    local child = Widget({
        id = "child",
        x = 20, y = 30, w = 50, h = 50,
        mouseAction = function(self, ev)
            childSeenLocalX = ev.x
            return false -- Do not consume, let it bubble
        end
    })

    local parent = Widget.Container({
        id = "parent",
        x = 100, y = 100, w = 200, h = 200,
        kids = { child },
        mouseAction = function(self, ev)
            parentSeenLocalX = ev.x
            return true -- Consume at parent
        end
    })

    local consumed = child:handleEvent({ type = "mouseButton", x = 135, y = 145, button = "LEFT", isDown = true })
    assert_true(consumed, "parent consumed bubbled event")
    assert_eq(15, childSeenLocalX, "child received localized x (135 - 100 - 20)")
    assert_eq(35, parentSeenLocalX, "parent received localized x (135 - 100)")
end

function Tests.test_setbox_rule_dispatch()
    print("test_setbox_rule_dispatch")
    setbox.clear()
    events.init()

    setbox.rule {
        id = "vfo-wheel-rule",
        tags = { "event.MouseWheel", "widget.VFOControl" },
        priority = 10,
        apply = { handler = "vfo_wheel", step = 500 }
    }

    local handledStep = nil
    events.registerHandler("vfo_wheel", function(event, widget, props)
        handledStep = props.step * event.delta
        return true
    end)

    local vfoWidget = Widget({ id = "vfo", tags = { "widget.VFOControl" } })
    local ev = { type = "mouseWheel", delta = 2 }
    local handled = events.dispatch(ev, vfoWidget)

    assert_true(handled, "dispatched via SetBox rule")
    assert_eq(1000, handledStep, "resolved step property from rule")
end

-- =============================================================================
-- Run all tests
-- =============================================================================

function Tests.runAll()
    print("\n=== Events Module Tests ===\n")
    passed = 0
    failed = 0

    Tests.test_init_and_register_handler()
    Tests.test_mode_tags()
    Tests.test_create_event_and_types()
    Tests.test_widget_hit_test_and_hover_tags()
    Tests.test_hierarchical_event_bubbling()
    Tests.test_setbox_rule_dispatch()

    print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
    return failed == 0
end

return Tests
