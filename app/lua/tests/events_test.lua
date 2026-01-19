--[[
    Unit tests for events.lua module
]]

local events = require("events")

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

function Tests.test_init()
    print("test_init")
    events.init()

    assert_eq(0, #(events.widgets or {}), "widgets empty after init")
    assert_eq(1, events.nextZIndex, "zIndex reset to 1")
    assert_nil(events._currentLayoutParent, "no current layout parent")
end

function Tests.test_register_widget()
    print("test_register_widget")
    events.init()

    events.registerWidget("btn1", {x = 10, y = 20, w = 100, h = 32}, {"Button", "Primary"})

    local widget = events.widgets["btn1"]
    assert_true(widget ~= nil, "widget registered")
    assert_eq(10, widget.bounds.x, "widget bounds x")
    assert_eq(20, widget.bounds.y, "widget bounds y")
    assert_eq(100, widget.bounds.w, "widget bounds w")
    assert_eq(32, widget.bounds.h, "widget bounds h")
    assert_eq(2, #widget.tags, "widget has 2 tags")
    assert_eq("Button", widget.tags[1], "first tag is Button")
    assert_eq(1, widget.zIndex, "widget zIndex is 1")
end

function Tests.test_zindex_increments()
    print("test_zindex_increments")
    events.init()

    events.registerWidget("w1", {x = 0, y = 0, w = 50, h = 50}, {})
    events.registerWidget("w2", {x = 0, y = 0, w = 50, h = 50}, {})
    events.registerWidget("w3", {x = 0, y = 0, w = 50, h = 50}, {})

    assert_eq(1, events.widgets["w1"].zIndex, "w1 zIndex")
    assert_eq(2, events.widgets["w2"].zIndex, "w2 zIndex")
    assert_eq(3, events.widgets["w3"].zIndex, "w3 zIndex")
end

function Tests.test_hit_testing()
    print("test_hit_testing")
    events.init()

    events.registerWidget("btn1", {x = 10, y = 10, w = 100, h = 50}, {"Button"})
    events.registerWidget("btn2", {x = 200, y = 10, w = 100, h = 50}, {"Button"})

    -- Hit btn1
    local hit = events.getWidgetAt(50, 30)
    assert_true(hit ~= nil, "hit widget found")
    assert_eq("btn1", hit.id, "hit correct widget")

    -- Hit btn2
    hit = events.getWidgetAt(250, 30)
    assert_true(hit ~= nil, "hit widget found")
    assert_eq("btn2", hit.id, "hit correct widget")

    -- Miss both
    hit = events.getWidgetAt(150, 30)
    assert_nil(hit, "no hit in gap")

    -- Miss above
    hit = events.getWidgetAt(50, 5)
    assert_nil(hit, "no hit above")
end

function Tests.test_hit_testing_zorder()
    print("test_hit_testing_zorder")
    events.init()

    -- Two overlapping widgets
    events.registerWidget("bottom", {x = 0, y = 0, w = 100, h = 100}, {"Panel"})
    events.registerWidget("top", {x = 50, y = 50, w = 100, h = 100}, {"Button"})

    -- Hit overlap area - should get top widget (higher zIndex)
    local hit = events.getWidgetAt(75, 75)
    assert_eq("top", hit.id, "hit top widget in overlap")

    -- Hit bottom-only area
    hit = events.getWidgetAt(25, 25)
    assert_eq("bottom", hit.id, "hit bottom widget")

    -- Hit top-only area
    hit = events.getWidgetAt(125, 125)
    assert_eq("top", hit.id, "hit top widget")
end

function Tests.test_point_in_widget()
    print("test_point_in_widget")
    events.init()

    events.registerWidget("btn", {x = 100, y = 100, w = 50, h = 30}, {})

    assert_true(events.isPointInWidget(110, 110, "btn"), "point inside")
    assert_true(events.isPointInWidget(100, 100, "btn"), "point at corner")
    assert_false(events.isPointInWidget(99, 110, "btn"), "point left of widget")
    assert_false(events.isPointInWidget(150, 110, "btn"), "point right of widget")
    assert_false(events.isPointInWidget(110, 99, "btn"), "point above widget")
    assert_false(events.isPointInWidget(110, 130, "btn"), "point below widget")
    assert_false(events.isPointInWidget(110, 110, "nonexistent"), "nonexistent widget")
end

function Tests.test_update_widget_bounds()
    print("test_update_widget_bounds")
    events.init()

    events.registerWidget("btn", {x = 0, y = 0, w = 100, h = 32}, {})

    events.updateWidgetBounds("btn", {x = 50, y = 60, w = 80, h = 40})

    local widget = events.widgets["btn"]
    assert_eq(50, widget.bounds.x, "updated x")
    assert_eq(60, widget.bounds.y, "updated y")
    assert_eq(80, widget.bounds.w, "updated w")
    assert_eq(40, widget.bounds.h, "updated h")
end

function Tests.test_unregister_widget()
    print("test_unregister_widget")
    events.init()

    events.registerWidget("btn", {x = 0, y = 0, w = 100, h = 32}, {})
    assert_true(events.widgets["btn"] ~= nil, "widget exists")

    events.unregisterWidget("btn")
    assert_nil(events.widgets["btn"], "widget removed")
end

function Tests.test_clear_widgets()
    print("test_clear_widgets")
    events.init()

    events.registerWidget("w1", {x = 0, y = 0, w = 50, h = 50}, {})
    events.registerWidget("w2", {x = 0, y = 0, w = 50, h = 50}, {})
    events.registerWidget("w3", {x = 0, y = 0, w = 50, h = 50}, {})

    events.clearWidgets()

    assert_nil(events.widgets["w1"], "w1 cleared")
    assert_nil(events.widgets["w2"], "w2 cleared")
    assert_nil(events.widgets["w3"], "w3 cleared")
    assert_eq(1, events.nextZIndex, "zIndex reset")
end

function Tests.test_layout_parent_stack()
    print("test_layout_parent_stack")
    events.init()

    assert_nil(events.getCurrentLayoutParent(), "no parent initially")

    events.pushLayoutParent("region_1")
    assert_eq("region_1", events.getCurrentLayoutParent(), "parent after push")

    events.pushLayoutParent("region_2")
    assert_eq("region_2", events.getCurrentLayoutParent(), "nested parent")

    events.popLayoutParent()
    assert_eq("region_1", events.getCurrentLayoutParent(), "parent after pop")

    events.popLayoutParent()
    assert_nil(events.getCurrentLayoutParent(), "no parent after all pops")
end

function Tests.test_widget_parent_from_layout()
    print("test_widget_parent_from_layout")
    events.init()

    events.pushLayoutParent("sidebar")
    events.registerWidget("btn1", {x = 0, y = 0, w = 50, h = 50}, {"Button"})
    assert_eq("sidebar", events.widgets["btn1"].parent, "widget inherits layout parent")

    events.popLayoutParent()
    events.registerWidget("btn2", {x = 0, y = 0, w = 50, h = 50}, {"Button"})
    assert_nil(events.widgets["btn2"].parent, "widget has no parent")
end

function Tests.test_widget_explicit_parent()
    print("test_widget_explicit_parent")
    events.init()

    events.pushLayoutParent("default_parent")
    events.registerWidget("btn", {x = 0, y = 0, w = 50, h = 50}, {"Button"}, "explicit_parent")
    assert_eq("explicit_parent", events.widgets["btn"].parent, "explicit parent overrides layout")
    events.popLayoutParent()
end

function Tests.test_register_handler()
    print("test_register_handler")
    events.init()

    local called = false
    events.registerHandler("test_handler", function(event, widget)
        called = true
        return true
    end)

    assert_true(events.handlers["test_handler"] ~= nil, "handler registered")
    events.handlers["test_handler"]({}, nil)
    assert_true(called, "handler was called")
end

function Tests.test_unregister_handler()
    print("test_unregister_handler")
    events.init()

    events.registerHandler("test_handler", function() return true end)
    assert_true(events.handlers["test_handler"] ~= nil, "handler exists")

    events.unregisterHandler("test_handler")
    assert_nil(events.handlers["test_handler"], "handler removed")
end

function Tests.test_build_event_tags()
    print("test_build_event_tags")
    events.init()

    local event = {
        type = "MouseDown",
        x = 100, y = 100,
        modifiers = {"Shift", "Ctrl"}
    }

    local widget = {
        id = "btn",
        tags = {"Button", "Primary"}
    }

    local tags = events._buildEventTags(event, widget)

    assert_true(#tags >= 5, "at least 5 tags")
    assert_eq("Event", tags[1], "first tag is Event")
    assert_eq("MouseDown", tags[2], "second tag is event type")
    -- Widget tags
    local hasButton = false
    local hasPrimary = false
    local hasShift = false
    local hasCtrl = false
    for _, t in ipairs(tags) do
        if t == "Button" then hasButton = true end
        if t == "Primary" then hasPrimary = true end
        if t == "Shift" then hasShift = true end
        if t == "Ctrl" then hasCtrl = true end
    end
    assert_true(hasButton, "has Button tag")
    assert_true(hasPrimary, "has Primary tag")
    assert_true(hasShift, "has Shift modifier")
    assert_true(hasCtrl, "has Ctrl modifier")
end

function Tests.test_build_event_tags_no_widget()
    print("test_build_event_tags_no_widget")
    events.init()

    local event = {
        type = "KeyDown",
        modifiers = {}
    }

    local tags = events._buildEventTags(event, nil)
    assert_eq(2, #tags, "only 2 tags without widget")
    assert_eq("Event", tags[1], "first tag is Event")
    assert_eq("KeyDown", tags[2], "second tag is event type")
end

function Tests.test_create_event()
    print("test_create_event")
    events.init()

    local event = events.createEvent(events.Type.MOUSE_DOWN, {
        x = 150,
        y = 200,
        button = 1
    })

    assert_eq("MouseDown", event.type, "event type")
    assert_eq(150, event.x, "event x")
    assert_eq(200, event.y, "event y")
    assert_eq(1, event.button, "event button")
    assert_true(event.timestamp ~= nil, "event has timestamp")
    assert_true(event.modifiers ~= nil, "event has modifiers array")
end

function Tests.test_event_types()
    print("test_event_types")

    assert_eq("MouseDown", events.Type.MOUSE_DOWN, "MOUSE_DOWN")
    assert_eq("MouseUp", events.Type.MOUSE_UP, "MOUSE_UP")
    assert_eq("MouseMove", events.Type.MOUSE_MOVE, "MOUSE_MOVE")
    assert_eq("MouseWheel", events.Type.MOUSE_WHEEL, "MOUSE_WHEEL")
    assert_eq("KeyDown", events.Type.KEY_DOWN, "KEY_DOWN")
    assert_eq("KeyUp", events.Type.KEY_UP, "KEY_UP")
    assert_eq("TextInput", events.Type.TEXT_INPUT, "TEXT_INPUT")
    assert_eq("WindowResize", events.Type.WINDOW_RESIZE, "WINDOW_RESIZE")
end

-- =============================================================================
-- Run all tests
-- =============================================================================

function Tests.runAll()
    print("\n=== Events Module Tests ===\n")
    passed = 0
    failed = 0

    Tests.test_init()
    Tests.test_register_widget()
    Tests.test_zindex_increments()
    Tests.test_hit_testing()
    Tests.test_hit_testing_zorder()
    Tests.test_point_in_widget()
    Tests.test_update_widget_bounds()
    Tests.test_unregister_widget()
    Tests.test_clear_widgets()
    Tests.test_layout_parent_stack()
    Tests.test_widget_parent_from_layout()
    Tests.test_widget_explicit_parent()
    Tests.test_register_handler()
    Tests.test_unregister_handler()
    Tests.test_build_event_tags()
    Tests.test_build_event_tags_no_widget()
    Tests.test_create_event()
    Tests.test_event_types()

    print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
    return failed == 0
end

return Tests
