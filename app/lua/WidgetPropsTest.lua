--[[
    WidgetPropsTest.lua - Tests for reactive widget properties
]]

local WidgetProps = require("WidgetProps")
local R = require("Reactive")

local passed = 0
local failed = 0

local function test(name, fn)
    -- Clear widgets between tests
    WidgetProps.clear()

    local ok, err = pcall(fn)
    if ok then
        passed = passed + 1
        print("✓ " .. name)
    else
        failed = failed + 1
        print("✗ " .. name .. ": " .. tostring(err))
    end
end

local function assertEqual(a, b, msg)
    if a ~= b then
        error((msg or "assertEqual") .. ": expected " .. tostring(b) .. ", got " .. tostring(a))
    end
end

-- =============================================================================
-- Basic Widget Tests
-- =============================================================================

test("widget: create and get property", function()
    local w = WidgetProps.create("test", { x = 10, y = 20 })
    assertEqual(w:get("x"), 10)
    assertEqual(w:get("y"), 20)
end)

test("widget: set property", function()
    local w = WidgetProps.create("test", { x = 10 })
    w:set("x", 50)
    assertEqual(w:get("x"), 50)
end)

test("widget: create new property via set", function()
    local w = WidgetProps.create("test", {})
    w:set("width", 100)
    assertEqual(w:get("width"), 100)
end)

test("widget: get by id", function()
    local w = WidgetProps.create("sidebar", { width = 260 })
    local found = WidgetProps.get("sidebar")
    assertEqual(found, w)
    assertEqual(found:get("width"), 260)
end)

-- =============================================================================
-- Computed Property Tests
-- =============================================================================

test("computed: derive from own properties", function()
    local w = WidgetProps.create("rect", { width = 10, height = 5 })
    w:computed("area", function()
        return w:get("width") * w:get("height")
    end)
    assertEqual(w:get("area"), 50)
end)

test("computed: updates when dependency changes", function()
    local w = WidgetProps.create("rect", { width = 10, height = 5 })
    w:computed("area", function()
        return w:get("width") * w:get("height")
    end)
    assertEqual(w:get("area"), 50)

    w:set("width", 20)
    assertEqual(w:get("area"), 100)
end)

test("computed: derive from other widget", function()
    local sidebar = WidgetProps.create("sidebar", { width = 260 })
    local main = WidgetProps.create("main", { y = 0 })

    main:computed("x", function()
        return sidebar:get("width")
    end)

    assertEqual(main:get("x"), 260)

    sidebar:set("width", 300)
    assertEqual(main:get("x"), 300)
end)

test("computed: chain across widgets", function()
    local a = WidgetProps.create("a", { value = 10 })
    local b = WidgetProps.create("b", {})
    local c = WidgetProps.create("c", {})

    b:computed("value", function()
        return a:get("value") * 2
    end)
    c:computed("value", function()
        return b:get("value") + 5
    end)

    assertEqual(c:get("value"), 25)  -- (10*2) + 5

    a:set("value", 20)
    assertEqual(c:get("value"), 45)  -- (20*2) + 5
end)

test("computed: cannot set computed property", function()
    local w = WidgetProps.create("test", { x = 10 })
    w:computed("doubled", function()
        return w:get("x") * 2
    end)

    local ok = pcall(function()
        w:set("doubled", 100)
    end)
    assertEqual(ok, false, "Should throw on setting computed")
end)

-- =============================================================================
-- Watch Tests
-- =============================================================================

test("watch: property changes", function()
    local w = WidgetProps.create("test", { x = 10 })
    local values = {}

    w:watch("x", function(value)
        table.insert(values, value)
    end)

    assertEqual(#values, 1)  -- Initial run
    assertEqual(values[1], 10)

    w:set("x", 20)
    assertEqual(#values, 2)
    assertEqual(values[2], 20)
end)

test("watch: computed property", function()
    local w = WidgetProps.create("test", { x = 10 })
    w:computed("doubled", function()
        return w:get("x") * 2
    end)

    local values = {}
    w:watch("doubled", function(value)
        table.insert(values, value)
    end)

    assertEqual(values[1], 20)

    w:set("x", 15)
    assertEqual(values[2], 30)
end)

-- =============================================================================
-- Tag Tests
-- =============================================================================

test("tags: add and check", function()
    local w = WidgetProps.create("test", {}, {"widget.Button"})
    assertEqual(w:hasTag("widget.Button"), true)
    assertEqual(w:hasTag("widget.Slider"), false)

    w:addTag("state.Hovered")
    assertEqual(w:hasTag("state.Hovered"), true)
end)

test("tags: remove", function()
    local w = WidgetProps.create("test", {}, {"widget.Button", "state.Hovered"})
    w:removeTag("state.Hovered")
    assertEqual(w:hasTag("state.Hovered"), false)
    assertEqual(w:hasTag("widget.Button"), true)
end)

-- =============================================================================
-- Batch Tests
-- =============================================================================

test("batch: groups updates", function()
    local w = WidgetProps.create("test", { x = 10, y = 20 })
    local runCount = 0

    w:computed("sum", function()
        return w:get("x") + w:get("y")
    end)

    R.watch(function()
        runCount = runCount + 1
        local _ = w:get("sum")
    end)
    assertEqual(runCount, 1)

    WidgetProps.batch(function()
        w:set("x", 100)
        w:set("y", 200)
    end)

    -- Watcher should have run just once after batch
    assertEqual(runCount, 2)
    assertEqual(w:get("sum"), 300)
end)

-- =============================================================================
-- Serialization Tests
-- =============================================================================

test("toTable: serializes observables", function()
    local w = WidgetProps.create("sidebar", { x = 0, width = 260 }, {"widget.Sidebar"})
    w:computed("right", function()
        return w:get("x") + w:get("width")
    end)

    local t = w:toTable()
    assertEqual(t.id, "sidebar")
    assertEqual(t.props.x, 0)
    assertEqual(t.props.width, 260)
    assertEqual(t.props.right, nil)  -- Computeds not serialized
    assertEqual(t.tags[1], "widget.Sidebar")
end)

-- =============================================================================
-- Cross-Widget Computed Tests
-- =============================================================================

test("cross-widget: layout dependency chain", function()
    -- Simulates: sidebar -> main panel -> nested widget
    local sidebar = WidgetProps.create("sidebar", { x = 0, width = 260 })
    local main = WidgetProps.create("main", { width = 800 })
    local nested = WidgetProps.create("nested", {})

    main:computed("x", function()
        return sidebar:get("x") + sidebar:get("width")
    end)

    nested:computed("x", function()
        return main:get("x") + 10  -- 10px padding
    end)

    assertEqual(nested:get("x"), 270)  -- 0 + 260 + 10

    sidebar:set("width", 300)
    assertEqual(main:get("x"), 300)
    assertEqual(nested:get("x"), 310)
end)

-- =============================================================================
-- Results
-- =============================================================================

print("")
print(string.format("Results: %d passed, %d failed", passed, failed))

if failed > 0 then
    os.exit(1)
end
