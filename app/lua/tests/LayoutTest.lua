--[[
    Unit tests for ui.layout module
]]

local layout = require("ui.Layout")

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

function Tests.test_begin_creates_root_region()
    print("test_begin_creates_root_region")
    layout.begin(0, 0, 800, 600)

    local x, y, w, h = layout.getRect()
    assert_eq(0, x, "root x")
    assert_eq(0, y, "root y")
    assert_eq(800, w, "root w")
    assert_eq(600, h, "root h")

    assert_eq(1, layout.getDepth(), "stack depth")

    layout.finish()
end

function Tests.test_dock_top()
    print("test_dock_top")
    layout.begin(0, 0, 800, 600)

    layout.dock("top", 40)
    local x, y, w, h = layout.getRect()
    assert_eq(0, x, "docked x")
    assert_eq(0, y, "docked y")
    assert_eq(800, w, "docked w")
    assert_eq(40, h, "docked h")
    layout.endDock()

    -- Remaining region should be reduced
    x, y, w, h = layout.getRect()
    assert_eq(0, x, "remaining x")
    assert_eq(40, y, "remaining y")
    assert_eq(800, w, "remaining w")
    assert_eq(560, h, "remaining h")

    layout.finish()
end

function Tests.test_dock_bottom()
    print("test_dock_bottom")
    layout.begin(0, 0, 800, 600)

    layout.dock("bottom", 30)
    local x, y, w, h = layout.getRect()
    assert_eq(0, x, "docked x")
    assert_eq(570, y, "docked y")
    assert_eq(800, w, "docked w")
    assert_eq(30, h, "docked h")
    layout.endDock()

    -- Remaining region
    x, y, w, h = layout.getRect()
    assert_eq(0, x, "remaining x")
    assert_eq(0, y, "remaining y")
    assert_eq(800, w, "remaining w")
    assert_eq(570, h, "remaining h")

    layout.finish()
end

function Tests.test_dock_left()
    print("test_dock_left")
    layout.begin(0, 0, 800, 600)

    layout.dock("left", 200)
    local x, y, w, h = layout.getRect()
    assert_eq(0, x, "docked x")
    assert_eq(0, y, "docked y")
    assert_eq(200, w, "docked w")
    assert_eq(600, h, "docked h")
    layout.endDock()

    -- Remaining region
    x, y, w, h = layout.getRect()
    assert_eq(200, x, "remaining x")
    assert_eq(0, y, "remaining y")
    assert_eq(600, w, "remaining w")
    assert_eq(600, h, "remaining h")

    layout.finish()
end

function Tests.test_dock_right()
    print("test_dock_right")
    layout.begin(0, 0, 800, 600)

    layout.dock("right", 150)
    local x, y, w, h = layout.getRect()
    assert_eq(650, x, "docked x")
    assert_eq(0, y, "docked y")
    assert_eq(150, w, "docked w")
    assert_eq(600, h, "docked h")
    layout.endDock()

    -- Remaining region
    x, y, w, h = layout.getRect()
    assert_eq(0, x, "remaining x")
    assert_eq(0, y, "remaining y")
    assert_eq(650, w, "remaining w")
    assert_eq(600, h, "remaining h")

    layout.finish()
end

function Tests.test_dock_all_sides()
    print("test_dock_all_sides")
    layout.begin(0, 0, 800, 600)

    layout.dock("top", 40)
    layout.endDock()

    layout.dock("bottom", 30)
    layout.endDock()

    layout.dock("left", 200)
    layout.endDock()

    layout.dock("right", 150)
    layout.endDock()

    -- Remaining center area
    local x, y, w, h = layout.getRect()
    assert_eq(200, x, "center x")
    assert_eq(40, y, "center y")
    assert_eq(450, w, "center w")  -- 800 - 200 - 150
    assert_eq(530, h, "center h")  -- 600 - 40 - 30

    layout.finish()
end

function Tests.test_nested_docks()
    print("test_nested_docks")
    layout.begin(0, 0, 800, 600)

    layout.dock("left", 300)
        -- Dock within dock
        layout.dock("top", 50)
        local x, y, w, h = layout.getRect()
        assert_eq(0, x, "nested dock x")
        assert_eq(0, y, "nested dock y")
        assert_eq(300, w, "nested dock w")
        assert_eq(50, h, "nested dock h")
        layout.endDock()

        -- Remaining after nested dock
        x, y, w, h = layout.getRect()
        assert_eq(0, x, "nested remaining x")
        assert_eq(50, y, "nested remaining y")
        assert_eq(300, w, "nested remaining w")
        assert_eq(550, h, "nested remaining h")
    layout.endDock()

    layout.finish()
end

function Tests.test_split_horizontal()
    print("test_split_horizontal")
    layout.begin(0, 0, 800, 600)

    layout.splitH(0.5)
    local x, y, w, h = layout.getRect()
    assert_eq(0, x, "left x")
    assert_eq(0, y, "left y")
    assert_eq(400, w, "left w")
    assert_eq(600, h, "left h")

    layout.nextSplit()
    x, y, w, h = layout.getRect()
    assert_eq(400, x, "right x")
    assert_eq(0, y, "right y")
    assert_eq(400, w, "right w")
    assert_eq(600, h, "right h")

    layout.endSplit()
    layout.finish()
end

function Tests.test_split_vertical()
    print("test_split_vertical")
    layout.begin(0, 0, 800, 600)

    layout.splitV(0.4)
    local x, y, w, h = layout.getRect()
    assert_eq(0, x, "top x")
    assert_eq(0, y, "top y")
    assert_eq(800, w, "top w")
    assert_eq(240, h, "top h")  -- 600 * 0.4 = 240

    layout.nextSplit()
    x, y, w, h = layout.getRect()
    assert_eq(0, x, "bottom x")
    assert_eq(240, y, "bottom y")
    assert_eq(800, w, "bottom w")
    assert_eq(360, h, "bottom h")  -- 600 - 240

    layout.endSplit()
    layout.finish()
end

function Tests.test_split_uneven()
    print("test_split_uneven")
    layout.begin(0, 0, 1000, 500)

    layout.splitH(0.25)
    local x, y, w, h = layout.getRect()
    assert_eq(250, w, "25% left w")

    layout.nextSplit()
    x, y, w, h = layout.getRect()
    assert_eq(750, w, "75% right w")

    layout.endSplit()
    layout.finish()
end

function Tests.test_horizontal_stacking()
    print("test_horizontal_stacking")
    layout.begin(0, 0, 800, 600)
    layout.beginHorizontal(10)

    local x1, y1 = layout.reserveSpace(100, 32)
    assert_eq(0, x1, "first item x")
    assert_eq(0, y1, "first item y")

    local x2, y2 = layout.reserveSpace(100, 32)
    assert_eq(110, x2, "second item x")  -- 100 + 10 spacing
    assert_eq(0, y2, "second item y")

    local x3, y3 = layout.reserveSpace(50, 32)
    assert_eq(220, x3, "third item x")  -- 110 + 100 + 10
    assert_eq(0, y3, "third item y")

    layout.endHorizontal()
    layout.finish()
end

function Tests.test_vertical_stacking()
    print("test_vertical_stacking")
    layout.begin(0, 0, 800, 600)
    layout.beginVertical(8)

    local x1, y1 = layout.reserveSpace(200, 28)
    assert_eq(0, x1, "first item x")
    assert_eq(0, y1, "first item y")

    local x2, y2 = layout.reserveSpace(200, 28)
    assert_eq(0, x2, "second item x")
    assert_eq(36, y2, "second item y")  -- 28 + 8 spacing

    local x3, y3 = layout.reserveSpace(200, 28)
    assert_eq(0, x3, "third item x")
    assert_eq(72, y3, "third item y")  -- 36 + 28 + 8

    layout.endVertical()
    layout.finish()
end

function Tests.test_pad_reduces_region()
    print("test_pad_reduces_region")
    layout.begin(0, 0, 800, 600)

    layout.pad(20)
    local x, y, w, h = layout.getRect()
    assert_eq(20, x, "padded x")
    assert_eq(20, y, "padded y")
    assert_eq(760, w, "padded w")  -- 800 - 40
    assert_eq(560, h, "padded h")  -- 600 - 40

    layout.finish()
end

function Tests.test_cursor_after_newline()
    print("test_cursor_after_newline")
    layout.begin(0, 0, 800, 600)

    local cx, cy = layout.getCursor()
    assert_eq(0, cx, "initial cursor x")
    assert_eq(0, cy, "initial cursor y")

    layout.newLine(30)
    cx, cy = layout.getCursor()
    assert_eq(0, cx, "cursor x after newline")
    assert_eq(34, cy, "cursor y after newline")  -- 30 + default spacing 4

    layout.finish()
end

function Tests.test_center_alignment()
    print("test_center_alignment")
    layout.begin(0, 0, 800, 600)

    local cx, cy = layout.center(100, 50)
    assert_eq(350, cx, "centered x")  -- (800 - 100) / 2
    assert_eq(275, cy, "centered y")  -- (600 - 50) / 2

    layout.finish()
end

function Tests.test_align_right()
    print("test_align_right")
    layout.begin(0, 0, 800, 600)

    local rx = layout.alignRight(150)
    assert_eq(650, rx, "right-aligned x")  -- 800 - 150

    layout.finish()
end

function Tests.test_align_bottom()
    print("test_align_bottom")
    layout.begin(0, 0, 800, 600)

    local by = layout.alignBottom(100)
    assert_eq(500, by, "bottom-aligned y")  -- 600 - 100

    layout.finish()
end

function Tests.test_indent_unindent()
    print("test_indent_unindent")
    layout.begin(0, 0, 800, 600)

    layout.indent(20)
    local cx, _ = layout.getCursor()
    assert_eq(20, cx, "cursor after indent")

    layout.indent(10)
    cx, _ = layout.getCursor()
    assert_eq(30, cx, "cursor after second indent")

    layout.unindent(15)
    cx, _ = layout.getCursor()
    assert_eq(15, cx, "cursor after unindent")

    layout.finish()
end

function Tests.test_space_in_horizontal()
    print("test_space_in_horizontal")
    layout.begin(0, 0, 800, 600)
    layout.beginHorizontal()

    local x1, _ = layout.reserveSpace(50, 20)
    assert_eq(0, x1, "first item x")

    layout.space(30)  -- Extra space

    local x2, _ = layout.reserveSpace(50, 20)
    -- 50 + 4 (default spacing) + 30 (extra) = 84
    assert_eq(84, x2, "item after space x")

    layout.endHorizontal()
    layout.finish()
end

function Tests.test_remaining_size()
    print("test_remaining_size")
    layout.begin(0, 0, 800, 600)
    layout.beginVertical()

    layout.reserveSpace(200, 100)
    local rw, rh = layout.getRemainingSize()
    assert_eq(800, rw, "remaining width")
    assert_eq(496, rh, "remaining height")  -- 600 - 100 - 4 spacing

    layout.endVertical()
    layout.finish()
end

function Tests.test_region_id_tracking()
    print("test_region_id_tracking")
    layout.begin(0, 0, 800, 600)

    local rootId = layout.getCurrentRegionId()
    assert_eq("region_1", rootId, "root region ID")

    layout.dock("top", 40, "header")
    local dockId = layout.getCurrentRegionId()
    assert_eq("region_2", dockId, "dock region ID")

    local dockName = layout.getCurrentRegionName()
    assert_eq("header", dockName, "dock region name")

    layout.endDock()
    layout.finish()
end

-- =============================================================================
-- Run all tests
-- =============================================================================

function Tests.runAll()
    print("\n=== Layout Module Tests ===\n")
    passed = 0
    failed = 0

    Tests.test_begin_creates_root_region()
    Tests.test_dock_top()
    Tests.test_dock_bottom()
    Tests.test_dock_left()
    Tests.test_dock_right()
    Tests.test_dock_all_sides()
    Tests.test_nested_docks()
    Tests.test_split_horizontal()
    Tests.test_split_vertical()
    Tests.test_split_uneven()
    Tests.test_horizontal_stacking()
    Tests.test_vertical_stacking()
    Tests.test_pad_reduces_region()
    Tests.test_cursor_after_newline()
    Tests.test_center_alignment()
    Tests.test_align_right()
    Tests.test_align_bottom()
    Tests.test_indent_unindent()
    Tests.test_space_in_horizontal()
    Tests.test_remaining_size()
    Tests.test_region_id_tracking()

    print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
    return failed == 0
end

return Tests
