--[[
    AppStateTest.lua - Tests for reactive application state
]]

-- Mock C++ APIs
rx = { setBandpassEnabled = function() end, setBandpassCenter = function() end, setBandpassWidth = function() end,
       setNotchEnabled = function() end, setNotchCenter = function() end, setNotchWidth = function() end,
       setAgcEnabled = function() end, setNrEnabled = function() end, setLmsMu = function() end, setMute = function() end }
audio = { setTestTone = function() end, setVolume = function() end }
hw = { setQsdOffset = function() end, setAttenuation = function() end, isConnected = function() return false end }
setbox = { getNumber = function(_, d) return d end, getString = function(_, d) return d end, getBool = function(_, d) return d end }
function getWindowSize() return 1600, 900 end

local R = require("Reactive")
local AppState = require("AppState")

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

-- Initialize
AppState.init()

-- =============================================================================
-- Basic Observable Access
-- =============================================================================

test("get: frequency returns value", function()
    local freq = AppState.get("frequency")
    assertEqual(type(freq), "number")
end)

test("set: frequency updates", function()
    AppState.set("frequency", 7.050)
    assertEqual(AppState.get("frequency"), 7.050)
end)

test("set: clamps to bounds", function()
    AppState.set("frequency", 100.0)  -- Max is 30.0
    assertEqual(AppState.get("frequency"), 30.0)

    AppState.set("frequency", 0.01)   -- Min is 0.1
    assertEqual(AppState.get("frequency"), 0.1)
end)

test("set: quantizes to step", function()
    AppState.set("rfAttenDb", 7)  -- Step is 3
    assertEqual(AppState.get("rfAttenDb"), 6)  -- Rounds to nearest 3

    AppState.set("rfAttenDb", 8)
    assertEqual(AppState.get("rfAttenDb"), 9)  -- Rounds to nearest 3
end)

-- =============================================================================
-- Computed Properties
-- =============================================================================

test("computed: frequencyDisplay updates", function()
    AppState.set("frequency", 14.200)
    local display = AppState.get("frequencyDisplay")
    assertEqual(display, "14.200000")
end)

test("computed: activeVfoFrequency follows activeVFO", function()
    AppState.set("vfoA", 14.200)
    AppState.set("vfoB", 7.050)
    AppState.set("activeVFO", "A")
    assertEqual(AppState.get("activeVfoFrequency"), 14.200)

    AppState.set("activeVFO", "B")
    assertEqual(AppState.get("activeVfoFrequency"), 7.050)
end)

-- =============================================================================
-- Watch
-- =============================================================================

test("watch: callback fires on change", function()
    local values = {}
    local watcher = AppState.watch("frequency", function(value)
        table.insert(values, value)
    end)

    -- Initial run
    assertEqual(#values, 1)

    AppState.set("frequency", 21.300)
    assertEqual(#values, 2)
    assertEqual(values[2], 21.300)

    watcher:dispose()
end)

test("watch: computed triggers on dependency change", function()
    local displays = {}
    local watcher = AppState.watch("frequencyDisplay", function(value)
        table.insert(displays, value)
    end)

    AppState.set("frequency", 28.500)
    assertEqual(#displays, 2)
    assertEqual(displays[2], "28.500000")

    watcher:dispose()
end)

-- =============================================================================
-- Batch
-- =============================================================================

test("batch: groups updates", function()
    local runCount = 0
    local watcher = AppState.watch("frequency", function()
        runCount = runCount + 1
    end)
    assertEqual(runCount, 1)  -- Initial run

    AppState.batch(function()
        AppState.set("frequency", 3.5)
        AppState.set("frequency", 7.0)
        AppState.set("frequency", 14.0)
    end)

    -- Should have run once more after batch (not 3 times)
    assertEqual(runCount, 2)

    watcher:dispose()
end)

-- =============================================================================
-- Metatable Access
-- =============================================================================

test("metatable: AppState.frequency is observable", function()
    local freq = AppState.frequency
    assertEqual(R.isObservable(freq), true)
    assertEqual(freq:get(), AppState.get("frequency"))
end)

test("metatable: AppState.frequencyDisplay is computed", function()
    local display = AppState.frequencyDisplay
    assertEqual(R.isComputed(display), true)
end)

-- =============================================================================
-- Serialization
-- =============================================================================

test("toTable: serializes all observables", function()
    AppState.set("frequency", 14.250)
    AppState.set("selectedMode", "LSB")

    local data = AppState.toTable()
    assertEqual(data.frequency, 14.250)
    assertEqual(data.selectedMode, "LSB")
end)

test("fromTable: restores state", function()
    AppState.fromTable({
        frequency = 3.750,
        selectedMode = "CW",
    })

    assertEqual(AppState.get("frequency"), 3.750)
    assertEqual(AppState.get("selectedMode"), "CW")
end)

-- =============================================================================
-- Results
-- =============================================================================

print("")
print(string.format("Results: %d passed, %d failed", passed, failed))

if failed > 0 then
    os.exit(1)
end
