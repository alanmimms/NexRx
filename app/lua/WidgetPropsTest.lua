--[[
    WidgetPropsTest.lua - Tests for unified reactive widget properties (ui.Widget + w.props)
]]

local Widget = require("ui.Widget")
local R = require("Reactive")
local setbox = require("SetBox")

local passed = 0
local failed = 0

local function test(name, fn)
    setbox.clear()
    -- Restore base widget defaults rule
    setbox.rule {
        id = "widget-base-defaults",
        tags = { "widget.Widget" },
        priority = -1000,
        apply = {
            flexW = 0,
            flexH = 0,
            minW = 0,
            minH = 0,
            prefW = 0,
            prefH = 0,
            maxW = 10000,
            maxH = 10000,
            margin = { left = 0, right = 0, top = 0, bottom = 0 },
            stick = Widget.Stick.L | Widget.Stick.T,
            opacity = 1.0,
        }
    }

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

local function createWidget(id, initialProps, tags)
    return Widget({
        id = id,
        props = initialProps or {},
        tags = tags or {},
    })
end

-- =============================================================================
-- Basic Widget Tests
-- =============================================================================

test("widget: create and get property", function()
    local w = createWidget("test", { x = 10, y = 20 })
    assertEqual(w:get("x"), 10)
    assertEqual(w:get("y"), 20)
    assertEqual(w.props.x, 10)
    assertEqual(w.props.y, 20)
end)

test("widget: set property", function()
    local w = createWidget("test", { x = 10 })
    w:set("x", 50)
    assertEqual(w:get("x"), 50)
    w.props.x = 75
    assertEqual(w.props.x, 75)
end)

test("widget: create new property via set", function()
    local w = createWidget("test", {})
    w:set("width", 100)
    assertEqual(w:get("width"), 100)
    assertEqual(w.props.width, 100)
end)

test("widget: unified id and name", function()
    local w = Widget({ name = "sidebar", props = { width = 260 } })
    assertEqual(w.id, "sidebar")
    assertEqual(w.name, "sidebar")
    assertEqual(w:hasTag("id.sidebar"), true)
    assertEqual(w.props.width, 260)
end)

test("widget: automatic class hierarchy tagging", function()
    local col = Widget.Column({ id = "my-col" })
    assertEqual(col:hasTag("widget.Column"), true)
    assertEqual(col:hasTag("widget.Container"), true)
    assertEqual(col:hasTag("widget.Widget"), true)
    assertEqual(col:hasTag("id.my-col"), true)
end)

-- =============================================================================
-- Computed Property Tests
-- =============================================================================

test("computed: derive from own properties", function()
    local w = createWidget("rect", { width = 10, height = 5 })
    w:computed("area", function()
        return w.props.width * w.props.height
    end)
    assertEqual(w.props.area, 50)
end)

test("computed: updates when dependency changes", function()
    local w = createWidget("rect", { width = 10, height = 5 })
    w.props:computed("area", function()
        return w.props.width * w.props.height
    end)
    assertEqual(w.props.area, 50)

    w.props.width = 20
    assertEqual(w.props.area, 100)
end)

test("computed: derive from other widget", function()
    local sidebar = createWidget("sidebar", { width = 260 })
    local main = createWidget("main", { y = 0 })

    main.props:computed("x", function()
        return sidebar.props.width
    end)

    assertEqual(main.props.x, 260)

    sidebar.props.width = 300
    assertEqual(main.props.x, 300)
end)

test("computed: chain across widgets", function()
    local a = createWidget("a", { value = 10 })
    local b = createWidget("b", {})
    local c = createWidget("c", {})

    b:computed("value", function()
        return a:get("value") * 2
    end)
    c:computed("value", function()
        return b:get("value") + 5
    end)

    assertEqual(c:get("value"), 25)

    a:set("value", 20)
    assertEqual(c:get("value"), 45)
end)

test("computed: cannot set computed property", function()
    local w = createWidget("test", { x = 10 })
    w:computed("doubled", function()
        return w:get("x") * 2
    end)

    local ok = pcall(function()
        w:set("doubled", 100)
    end)
    assertEqual(ok, false, "Should throw on setting computed")
end)

-- =============================================================================
-- SetBox Rule Fallback & Reactive Tag Tests
-- =============================================================================

test("rules: fallback to SetBox rules and reactive local tag override", function()
    setbox.rule {
        id = "btn-bg",
        tags = { "widget.Button" },
        priority = 1,
        apply = { background = "#3b82f6" }
    }
    setbox.rule {
        id = "btn-hover-bg",
        tags = { "widget.Button", "state.Hovered" },
        priority = 10,
        apply = { background = "#60a5fa" }
    }

    local w = createWidget("btn1", {}, { "widget.Button" })
    local observed = {}
    w:watch("background", function(val)
        table.insert(observed, val)
    end)

    assertEqual(w.props.background, "#3b82f6")
    assertEqual(#observed, 1)

    -- Toggling local tag reactively updates computed rule fallback
    w:addTag("state.Hovered")
    assertEqual(w.props.background, "#60a5fa")
    assertEqual(#observed, 2)
    assertEqual(observed[2], "#60a5fa")

    -- Local prop override shadows rule, clearing local prop restores rule
    w.props.background = "#ff0000"
    assertEqual(w.props.background, "#ff0000")
    w.props.background = nil
    assertEqual(w.props.background, "#60a5fa")
end)

-- =============================================================================
-- Watch Tests
-- =============================================================================

test("watch: property changes", function()
    local w = createWidget("test", { x = 10 })
    local values = {}

    w:watch("x", function(value)
        table.insert(values, value)
    end)

    assertEqual(#values, 1)
    assertEqual(values[1], 10)

    w:set("x", 20)
    assertEqual(#values, 2)
    assertEqual(values[2], 20)
end)

test("watch: computed property", function()
    local w = createWidget("test", { x = 10 })
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
    local w = createWidget("test", {}, {"widget.Button"})
    assertEqual(w:hasTag("widget.Button"), true)
    assertEqual(w:hasTag("widget.Slider"), false)

    w:addTag("state.Hovered")
    assertEqual(w:hasTag("state.Hovered"), true)
end)

test("tags: remove", function()
    local w = createWidget("test", {}, {"widget.Button", "state.Hovered"})
    w:removeTag("state.Hovered")
    assertEqual(w:hasTag("state.Hovered"), false)
    assertEqual(w:hasTag("widget.Button"), true)
end)

-- =============================================================================
-- Batch Tests
-- =============================================================================

test("batch: groups updates", function()
    local w = createWidget("test", { x = 10, y = 20 })
    local runCount = 0

    w:computed("sum", function()
        return w:get("x") + w:get("y")
    end)

    R.watch(function()
        runCount = runCount + 1
        local _ = w:get("sum")
    end)
    assertEqual(runCount, 1)

    Widget.batch(function()
        w:set("x", 100)
        w:set("y", 200)
    end)

    assertEqual(runCount, 2)
    assertEqual(w:get("sum"), 300)
end)

-- =============================================================================
-- Serialization Tests
-- =============================================================================

test("toTable: serializes observables", function()
    local w = createWidget("sidebar", { x = 0, width = 260 }, {"widget.Sidebar"})
    w:computed("right", function()
        return w:get("x") + w:get("width")
    end)

    local t = w:toTable()
    assertEqual(t.id, "sidebar")
    assertEqual(t.props.x, 0)
    assertEqual(t.props.width, 260)
    assertEqual(t.props.right, nil)
    assertEqual(w:hasTag("widget.Sidebar"), true)
end)

-- =============================================================================
-- Cross-Widget Computed Tests
-- =============================================================================

test("cross-widget: layout dependency chain", function()
    local sidebar = createWidget("sidebar", { x = 0, width = 260 })
    local main = createWidget("main", { width = 800 })
    local nested = createWidget("nested", {})

    main:computed("x", function()
        return sidebar:get("x") + sidebar:get("width")
    end)

    nested:computed("x", function()
        return main:get("x") + 10
    end)

    assertEqual(nested:get("x"), 270)

    sidebar:set("width", 300)
    assertEqual(main:get("x"), 300)
    assertEqual(nested:get("x"), 310)
end)

-- =============================================================================
-- Results
-- =============================================================================

local M = {}
function M.runAll()
    print(string.format("WidgetPropsTest Results: %d passed, %d failed", passed, failed))
    return failed == 0
end

if arg and arg[0] and arg[0]:match("WidgetPropsTest%.lua$") then
    print("")
    print(string.format("Results: %d passed, %d failed", passed, failed))
    if failed > 0 then
        os.exit(1)
    end
end

return M
