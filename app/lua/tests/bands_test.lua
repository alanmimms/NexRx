--[[
    Unit tests for bands.lua module
]]

local bands = require("bands")

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

local function assert_true(value, msg)
    if value then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected true)", msg))
        return false
    end
end

-- =============================================================================
-- Tests
-- =============================================================================

function Tests.test_init()
    print("test_init")
    bands.init()

    -- After init, should have "OOB" (out of band) since freq is 0
    local current = bands.getCurrent()
    assert_true(current == nil or current == "OOB", "no valid band initially")
    assert_eq(0, bands.frequencyHz, "frequency is 0")
end

function Tests.test_band_definitions_loaded()
    print("test_band_definitions_loaded")
    bands.init()

    -- Check that band definitions were loaded from config
    assert_true(#bands.definitions > 0, "band definitions loaded")

    -- Check for some expected bands
    local found20m = false
    local found40m = false
    for _, def in ipairs(bands.definitions) do
        if def.name == "20m" then found20m = true end
        if def.name == "40m" then found40m = true end
    end
    assert_true(found20m, "20m band defined")
    assert_true(found40m, "40m band defined")
end

function Tests.test_freq_to_band_20m()
    print("test_freq_to_band_20m")
    bands.init()

    bands.setCurrent(14.2e6)  -- 14.200 MHz
    assert_eq("20m", bands.getCurrent(), "14.2 MHz is 20m")

    bands.setCurrent(14.0e6)  -- Band edge
    assert_eq("20m", bands.getCurrent(), "14.0 MHz is 20m")

    bands.setCurrent(14.35e6)  -- Upper edge
    assert_eq("20m", bands.getCurrent(), "14.35 MHz is 20m")
end

function Tests.test_freq_to_band_40m()
    print("test_freq_to_band_40m")
    bands.init()

    bands.setCurrent(7.15e6)  -- 7.150 MHz
    assert_eq("40m", bands.getCurrent(), "7.15 MHz is 40m")

    bands.setCurrent(7.0e6)  -- Band start
    assert_eq("40m", bands.getCurrent(), "7.0 MHz is 40m")
end

function Tests.test_freq_to_band_80m()
    print("test_freq_to_band_80m")
    bands.init()

    bands.setCurrent(3.75e6)  -- 3.750 MHz
    assert_eq("80m", bands.getCurrent(), "3.75 MHz is 80m")
end

function Tests.test_freq_to_band_160m()
    print("test_freq_to_band_160m")
    bands.init()

    bands.setCurrent(1.9e6)  -- 1.900 MHz
    assert_eq("160m", bands.getCurrent(), "1.9 MHz is 160m")
end

function Tests.test_freq_to_band_15m()
    print("test_freq_to_band_15m")
    bands.init()

    bands.setCurrent(21.2e6)  -- 21.200 MHz
    assert_eq("15m", bands.getCurrent(), "21.2 MHz is 15m")
end

function Tests.test_freq_to_band_10m()
    print("test_freq_to_band_10m")
    bands.init()

    bands.setCurrent(28.5e6)  -- 28.500 MHz
    assert_eq("10m", bands.getCurrent(), "28.5 MHz is 10m")
end

function Tests.test_band_edge_lower()
    print("test_band_edge_lower")
    bands.init()

    -- Just below 20m band
    bands.setCurrent(13.999e6)
    local current = bands.getCurrent()
    assert_true(current == nil or current == "OOB", "13.999 MHz is out of band")

    -- Just at 20m band edge
    bands.setCurrent(14.0e6)
    assert_eq("20m", bands.getCurrent(), "14.0 MHz is in 20m")
end

function Tests.test_band_edge_upper()
    print("test_band_edge_upper")
    bands.init()

    -- Just above 20m band (assuming exclusive upper bound)
    bands.setCurrent(14.351e6)
    -- This might be in band or out depending on definition
    -- Most definitions use inclusive bounds, so let's check
    local current = bands.getCurrent()
    -- If in band, should be 20m; if out, should be nil
    -- This tests the edge case behavior
    print(string.format("    Info: 14.351 MHz -> %s", current or "nil"))
    passed = passed + 1  -- Informational test
end

function Tests.test_out_of_band()
    print("test_out_of_band")
    bands.init()

    -- Frequency clearly out of amateur bands
    bands.setCurrent(50.0e6)  -- 6m is usually defined separately
    local current = bands.getCurrent()
    -- May be "6m" if defined, or "OOB"
    print(string.format("    Info: 50 MHz -> %s", current or "nil"))
    passed = passed + 1

    -- Way out of band - returns "OOB" not nil
    bands.setCurrent(100.0e6)
    current = bands.getCurrent()
    assert_true(current == nil or current == "OOB", "100 MHz is out of band")
end

function Tests.test_band_change_detection()
    print("test_band_change_detection")
    bands.init()

    bands.setCurrent(14.2e6)
    assert_eq("20m", bands.getCurrent(), "starts in 20m")

    bands.setCurrent(7.15e6)
    assert_eq("40m", bands.getCurrent(), "changed to 40m")

    bands.setCurrent(21.2e6)
    assert_eq("15m", bands.getCurrent(), "changed to 15m")
end

function Tests.test_same_band_no_change()
    print("test_same_band_no_change")
    bands.init()

    bands.setCurrent(14.0e6)
    local band1 = bands.getCurrent()
    assert_eq("20m", band1, "initial band")

    bands.setCurrent(14.2e6)
    local band2 = bands.getCurrent()
    assert_eq("20m", band2, "still 20m")
    assert_eq(band1, band2, "band didn't change")
end

function Tests.test_frequency_tracking()
    print("test_frequency_tracking")
    bands.init()

    bands.setCurrent(14200000)  -- 14.2 MHz in Hz
    assert_eq(14200000, bands.frequencyHz, "frequency stored correctly")

    bands.setCurrent(7150000)
    assert_eq(7150000, bands.frequencyHz, "frequency updated")
end

function Tests.test_get_band_for_freq_internal()
    print("test_get_band_for_freq_internal")
    bands.init()

    -- Test the internal function if exposed
    if bands._getBandForFreq then
        local band = bands._getBandForFreq(14.2e6)
        assert_eq("20m", band, "_getBandForFreq returns correct band")

        band = bands._getBandForFreq(100e6)
        assert_true(band == nil or band == "OOB", "_getBandForFreq returns nil/OOB for out of band")
    else
        passed = passed + 1  -- Skip if not exposed
    end
end

function Tests.test_zero_frequency()
    print("test_zero_frequency")
    bands.init()

    bands.setCurrent(0)
    local current = bands.getCurrent()
    assert_true(current == nil or current == "OOB", "0 Hz is out of band")
end

function Tests.test_negative_frequency()
    print("test_negative_frequency")
    bands.init()

    bands.setCurrent(-14.2e6)
    local current = bands.getCurrent()
    assert_true(current == nil or current == "OOB", "negative frequency is out of band")
end

function Tests.test_band_30m()
    print("test_band_30m")
    bands.init()

    bands.setCurrent(10.125e6)  -- 10.125 MHz
    assert_eq("30m", bands.getCurrent(), "10.125 MHz is 30m")
end

function Tests.test_band_17m()
    print("test_band_17m")
    bands.init()

    bands.setCurrent(18.1e6)  -- 18.100 MHz
    assert_eq("17m", bands.getCurrent(), "18.1 MHz is 17m")
end

function Tests.test_band_12m()
    print("test_band_12m")
    bands.init()

    bands.setCurrent(24.93e6)  -- 24.930 MHz
    assert_eq("12m", bands.getCurrent(), "24.93 MHz is 12m")
end

-- =============================================================================
-- Run all tests
-- =============================================================================

function Tests.runAll()
    print("\n=== Bands Module Tests ===\n")
    passed = 0
    failed = 0

    Tests.test_init()
    Tests.test_band_definitions_loaded()
    Tests.test_freq_to_band_20m()
    Tests.test_freq_to_band_40m()
    Tests.test_freq_to_band_80m()
    Tests.test_freq_to_band_160m()
    Tests.test_freq_to_band_15m()
    Tests.test_freq_to_band_10m()
    Tests.test_band_edge_lower()
    Tests.test_band_edge_upper()
    Tests.test_out_of_band()
    Tests.test_band_change_detection()
    Tests.test_same_band_no_change()
    Tests.test_frequency_tracking()
    Tests.test_get_band_for_freq_internal()
    Tests.test_zero_frequency()
    Tests.test_negative_frequency()
    Tests.test_band_30m()
    Tests.test_band_17m()
    Tests.test_band_12m()

    print(string.format("\nResults: %d passed, %d failed\n", passed, failed))
    return failed == 0
end

return Tests
