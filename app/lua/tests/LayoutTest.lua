--[[
    Unit tests for retained-mode ui.Layout and Widget.Container layout
]]

local Layout = require("ui.Layout")
local Widget = require("ui.Widget")
local Stick = require("ui.Stick")
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

local function assert_close(expected, actual, tolerance, msg)
    tolerance = tolerance or 0.001
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
-- Tests
-- =============================================================================

function Tests.test_hFlow_fixed_and_flex()
    print("test_hFlow_fixed_and_flex")
    local k1 = Widget({ id = "k1", prefW = 100, prefH = 40, stick = "TLB" })
    local k2 = Widget({ id = "k2", flexW = 1, prefH = 40, stick = "TLBR" })
    local k3 = Widget({ id = "k3", flexW = 2, prefH = 40, stick = "TLBR" })

    local row = Widget.Row({
        id = "row",
        kids = { k1, k2, k3 },
    })

    row:layout(0, 0, 700, 100)

    assert_eq(0, k1.props.x, "k1 x")
    assert_eq(100, k1.props.w, "k1 w")
    assert_eq(100, k1.props.h, "k1 h stretched TLB")

    assert_close(100, k2.props.x, 0.01, "k2 x")
    assert_close(200, k2.props.w, 0.01, "k2 w (1/3 of 600)")
    assert_eq(100, k2.props.h, "k2 h")

    assert_close(300, k3.props.x, 0.01, "k3 x")
    assert_close(400, k3.props.w, 0.01, "k3 w (2/3 of 600)")
end

function Tests.test_vFlow_fixed_and_flex()
    print("test_vFlow_fixed_and_flex")
    local header = Widget({ id = "header", prefH = 40, stick = "TLR" })
    local body = Widget({ id = "body", flexH = 1, stick = "TLBR" })
    local footer = Widget({ id = "footer", prefH = 60, stick = "TLR" })

    local col = Widget.Column({
        id = "col",
        kids = { header, body, footer },
    })

    col:layout(0, 0, 800, 600)

    assert_eq(0, header.props.y, "header y")
    assert_eq(800, header.props.w, "header w")
    assert_eq(40, header.props.h, "header h")

    assert_eq(40, body.props.y, "body y")
    assert_eq(800, body.props.w, "body w")
    assert_eq(500, body.props.h, "body h (600 - 40 - 60)")

    assert_eq(540, footer.props.y, "footer y")
    assert_eq(60, footer.props.h, "footer h")
end

function Tests.test_margins_and_stick_alignment()
    print("test_margins_and_stick_alignment")
    local centered = Widget({
        id = "centered",
        prefW = 100,
        prefH = 50,
        stick = 0, -- Centered on both axes when neither L/R nor T/B is set
    })

    local stack = Widget.Container({
        id = "stack",
        layoutFunc = Layout.zStack,
        kids = { centered },
    })

    stack:layout(0, 0, 400, 200)
    assert_eq(150, centered.props.x, "zStack centered x")
    assert_eq(75, centered.props.y, "zStack centered y")
    assert_eq(100, centered.props.w, "zStack centered w")
    assert_eq(50, centered.props.h, "zStack centered h")

    local padded = Widget({
        id = "padded",
        stick = "TLBR",
        margin = 10,
    })
    local stack2 = Widget.Container({
        id = "stack2",
        layoutFunc = Layout.zStack,
        kids = { padded },
    })
    stack2:layout(0, 0, 400, 200)
    assert_eq(10, padded.props.x, "padded x")
    assert_eq(10, padded.props.y, "padded y")
    assert_eq(380, padded.props.w, "padded w")
    assert_eq(180, padded.props.h, "padded h")
end

function Tests.test_wrapFlow()
    print("test_wrapFlow")
    local w1 = Widget({ prefW = 120, prefH = 30 })
    local w2 = Widget({ prefW = 120, prefH = 40 })
    local w3 = Widget({ prefW = 120, prefH = 30 })

    local wrap = Widget.Container({
        layoutFunc = Layout.wrapFlow,
        kids = { w1, w2, w3 },
    })

    wrap:layout(0, 0, 250, 200)
    assert_eq(0, w1.props.x, "w1 x")
    assert_eq(0, w1.props.y, "w1 y")
    assert_eq(120, w2.props.x, "w2 x")
    assert_eq(0, w2.props.y, "w2 y")
    -- w3 wraps to next line (rowMaxH = 40)
    assert_eq(0, w3.props.x, "w3 wrapped x")
    assert_eq(40, w3.props.y, "w3 wrapped y")
end

function Tests.test_grid()
    print("test_grid")
    local g1 = Widget({ prefW = 50, prefH = 20 })
    local g2 = Widget({ prefW = 80, prefH = 30 })
    local g3 = Widget({ prefW = 60, prefH = 25 })
    local g4 = Widget({ prefW = 40, prefH = 20 })

    local grid = Widget.Container({
        layoutFunc = Layout.grid,
        cols = 2,
        kids = { g1, g2, g3, g4 },
    })

    grid:layout(0, 0, 300, 300)
    -- Col 1 max width = max(50, 60) = 60; Row 1 max height = max(20, 30) = 30
    assert_eq(0, g1.props.x, "g1 x")
    assert_eq(0, g1.props.y, "g1 y")
    assert_eq(60, g2.props.x, "g2 x")
    assert_eq(0, g2.props.y, "g2 y")
    assert_eq(0, g3.props.x, "g3 x")
    assert_eq(30, g3.props.y, "g3 y")
    assert_eq(60, g4.props.x, "g4 x")
    assert_eq(30, g4.props.y, "g4 y")
end

function Tests.test_rule_driven_metrics()
    print("test_rule_driven_metrics")
    setbox.rule {
        id = "test-sidebar-rule",
        tags = { "widget.TestSidebar" },
        priority = 10,
        apply = {
            prefW = 240,
            stick = "TLB",
        }
    }

    local sb = Widget({ id = "sb", tags = { "widget.TestSidebar" } })
    local center = Widget({ id = "center", flexW = 1, stick = "TLBR" })
    local root = Widget.Row({ kids = { sb, center } })

    root:layout(0, 0, 1000, 500)
    assert_eq(240, sb.props.w, "rule-resolved sidebar width")
    assert_eq(500, sb.props.h, "rule-resolved sidebar stick TLB height")
    assert_eq(240, center.props.x, "center x after sidebar")
    assert_eq(760, center.props.w, "center flex width")
end

-- =============================================================================
-- Run all tests
-- =============================================================================

function Tests.runAll()
    print("\n=== Layout Module Tests ===\n")
    passed = 0
    failed = 0

    Tests.test_hFlow_fixed_and_flex()
    Tests.test_vFlow_fixed_and_flex()
    Tests.test_margins_and_stick_alignment()
    Tests.test_wrapFlow()
    Tests.test_grid()
    Tests.test_rule_driven_metrics()

    print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
    return failed == 0
end

return Tests
