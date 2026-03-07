--[[
    ReactiveTest.lua - Tests for the reactive property system
]]

local R = require("Reactive")

local passed = 0
local failed = 0

local function test(name, fn)
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
-- Observable Tests
-- =============================================================================

test("observable: get/set basic value", function()
    local x = R.observable(10)
    assertEqual(x:get(), 10)
    x:set(20)
    assertEqual(x:get(), 20)
end)

test("observable: peek doesn't track", function()
    local x = R.observable(10)
    local readCount = 0
    local c = R.computed(function()
        readCount = readCount + 1
        return x:peek() * 2
    end)
    assertEqual(c:get(), 20)
    assertEqual(readCount, 1)
    x:set(20)
    -- Computed should NOT be dirty because we used peek()
    assertEqual(c:isDirty(), false)
    assertEqual(c:get(), 20)  -- Still cached value
    assertEqual(readCount, 1)  -- Didn't recompute
end)

-- =============================================================================
-- Computed Tests
-- =============================================================================

test("computed: derives from observable", function()
    local x = R.observable(5)
    local doubled = R.computed(function()
        return x:get() * 2
    end)
    assertEqual(doubled:get(), 10)
end)

test("computed: lazy recomputation", function()
    local x = R.observable(5)
    local computeCount = 0
    local doubled = R.computed(function()
        computeCount = computeCount + 1
        return x:get() * 2
    end)

    assertEqual(doubled:get(), 10)
    assertEqual(computeCount, 1)

    -- Multiple reads don't recompute
    assertEqual(doubled:get(), 10)
    assertEqual(doubled:get(), 10)
    assertEqual(computeCount, 1)

    -- Change triggers recompute on next read
    x:set(10)
    assertEqual(computeCount, 1)  -- Not yet recomputed
    assertEqual(doubled:get(), 20)
    assertEqual(computeCount, 2)  -- Now recomputed
end)

test("computed: chain of computeds", function()
    local x = R.observable(2)
    local doubled = R.computed(function() return x:get() * 2 end)
    local quadrupled = R.computed(function() return doubled:get() * 2 end)

    assertEqual(quadrupled:get(), 8)
    x:set(3)
    assertEqual(quadrupled:get(), 12)
end)

test("computed: diamond dependency", function()
    --     x
    --    / \
    --   a   b
    --    \ /
    --     c
    local x = R.observable(1)
    local computeA, computeB, computeC = 0, 0, 0

    local a = R.computed(function()
        computeA = computeA + 1
        return x:get() + 1
    end)
    local b = R.computed(function()
        computeB = computeB + 1
        return x:get() * 2
    end)
    local c = R.computed(function()
        computeC = computeC + 1
        return a:get() + b:get()
    end)

    assertEqual(c:get(), 4)  -- (1+1) + (1*2) = 4
    assertEqual(computeA, 1)
    assertEqual(computeB, 1)
    assertEqual(computeC, 1)

    x:set(2)
    assertEqual(c:get(), 7)  -- (2+1) + (2*2) = 7
    assertEqual(computeA, 2)
    assertEqual(computeB, 2)
    assertEqual(computeC, 2)
end)

test("computed: dynamic dependencies", function()
    local useX = R.observable(true)
    local x = R.observable(10)
    local y = R.observable(20)

    local value = R.computed(function()
        if useX:get() then
            return x:get()
        else
            return y:get()
        end
    end)

    assertEqual(value:get(), 10)

    -- Changing y shouldn't trigger recompute (not a dependency)
    y:set(30)
    assertEqual(value:isDirty(), false)

    -- Switch to using y
    useX:set(false)
    assertEqual(value:get(), 30)

    -- Now changing x shouldn't trigger recompute
    x:set(100)
    assertEqual(value:isDirty(), false)
    assertEqual(value:get(), 30)

    -- But changing y should
    y:set(40)
    assertEqual(value:get(), 40)
end)

-- =============================================================================
-- Watch Tests
-- =============================================================================

test("watch: runs immediately", function()
    local x = R.observable(5)
    local watchValue = nil
    local watcher = R.watch(function()
        watchValue = x:get()
    end)
    assertEqual(watchValue, 5)
    watcher:dispose()
end)

test("watch: runs on change", function()
    local x = R.observable(5)
    local runCount = 0
    local watcher = R.watch(function()
        runCount = runCount + 1
        local _ = x:get()
    end)
    assertEqual(runCount, 1)  -- Initial run

    x:set(10)
    assertEqual(runCount, 2)  -- Ran again

    x:set(10)  -- Same value
    assertEqual(runCount, 2)  -- Didn't run

    watcher:dispose()
    x:set(20)
    assertEqual(runCount, 2)  -- Disposed, didn't run
end)

-- =============================================================================
-- Batch Tests
-- =============================================================================

test("batch: groups updates", function()
    local x = R.observable(1)
    local y = R.observable(2)
    local runCount = 0

    local watcher = R.watch(function()
        runCount = runCount + 1
        local _ = x:get() + y:get()
    end)
    assertEqual(runCount, 1)

    R.batch(function()
        x:set(10)
        y:set(20)
    end)
    -- Should only run once after batch, not twice
    assertEqual(runCount, 2)

    watcher:dispose()
end)

-- =============================================================================
-- Observable Object Tests
-- =============================================================================

test("observableObject: property access", function()
    local obj = R.observableObject({ x = 10, y = 20 })
    assertEqual(obj.x, 10)
    assertEqual(obj.y, 20)

    obj.x = 30
    assertEqual(obj.x, 30)
end)

test("observableObject: reactive with computed", function()
    local obj = R.observableObject({ width = 10, height = 5 })
    local area = R.computed(function()
        return obj.width * obj.height
    end)

    assertEqual(area:get(), 50)
    obj.width = 20
    assertEqual(area:get(), 100)
end)

-- =============================================================================
-- Untrack Tests
-- =============================================================================

test("untrack: prevents dependency tracking", function()
    local x = R.observable(10)
    local computeCount = 0

    local c = R.computed(function()
        computeCount = computeCount + 1
        return R.untrack(function()
            return x:get() * 2
        end)
    end)

    assertEqual(c:get(), 20)
    assertEqual(computeCount, 1)

    x:set(20)
    -- Computed should NOT be dirty because read was untracked
    assertEqual(c:isDirty(), false)
end)

-- =============================================================================
-- Results
-- =============================================================================

print("")
print(string.format("Results: %d passed, %d failed", passed, failed))

if failed > 0 then
    os.exit(1)
end
