--[[
    Unit tests for setbox module

    Tests equivalent to C++ setbox_test.cpp, plus additional coverage.
]]

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

local function assert_true(condition, msg)
    if condition then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected true)", msg))
        return false
    end
end

local function assert_false(condition, msg)
    if not condition then
        passed = passed + 1
        return true
    else
        failed = failed + 1
        print(string.format("  FAIL: %s (expected false)", msg))
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

-- Helper: setup test configuration (matches C++ test config)
local function setupTestConfig()
    setbox._clear()

    -- Base defaults (lowest priority)
    setbox.rule {
        tags = {},
        priority = -100,
        apply = {
            background = "#1a1a2e",
            foreground = "#ffffff",
            fontSize = 14,
            borderRadius = 4,
        }
    }

    -- Button base style
    setbox.rule {
        tags = {"Button"},
        apply = {
            background = "#3b82f6",
            foreground = "#ffffff",
            padding = 8,
            borderRadius = 6,
        }
    }

    -- Primary button variant
    setbox.rule {
        tags = {"Button", "Primary"},
        apply = {
            background = "#2563eb",
            fontWeight = "bold",
        }
    }

    -- Secondary button variant
    setbox.rule {
        tags = {"Button", "Secondary"},
        apply = {
            background = "#64748b",
        }
    }

    -- Sidebar context
    setbox.rule {
        tags = {"Sidebar"},
        apply = {
            background = "#0f172a",
            width = 250,
        }
    }

    -- Button in sidebar gets different style
    setbox.rule {
        tags = {"Button", "Sidebar"},
        apply = {
            borderRadius = 0,
            width = "100%",
        }
    }

    -- Primary button in sidebar (most specific)
    setbox.rule {
        tags = {"Button", "Primary", "Sidebar"},
        apply = {
            background = "#1d4ed8",
        }
    }

    -- Experimental override (high priority)
    setbox.rule {
        tags = {"Button", "experimental"},
        priority = 100,
        apply = {
            background = "#dc2626",
            borderRadius = 20,
        }
    }

    -- Conditional rule (hover state)
    setbox.rule {
        tags = {"Button"},
        ["when"] = function(ctx) return ctx.hovered == true end,
        priority = 50,
        apply = {
            background = "#60a5fa",
            cursor = "pointer",
        }
    }

    -- Radio configuration
    setbox.rule {
        tags = {"Radio", "20m"},
        apply = {
            frequency = 14.0e6,
            antenna = "beam",
        }
    }

    setbox.rule {
        tags = {"Radio", "20m", "CW"},
        apply = {
            frequency = 14.035e6,
            mode = "CW",
            filterWidth = 500,
        }
    }

    setbox.rule {
        tags = {"Radio", "40m", "SSB"},
        apply = {
            frequency = 7.2e6,
            mode = "LSB",
            filterWidth = 2400,
        }
    }
end

-- =============================================================================
-- Test: Rule Registration
-- =============================================================================

function Tests.test_rule_registration()
    print("test_rule_registration")
    setbox._clear()

    setbox.rule { tags = {"Test"}, apply = { value = 42 } }
    local rules = setbox.getRules()

    assert_eq(1, #rules, "one rule registered")
    assert_eq(42, rules[1].properties.value, "rule property value")
end

function Tests.test_rule_with_id()
    print("test_rule_with_id")
    setbox._clear()

    setbox.rule { id = "my-custom-rule", tags = {"Test"}, apply = { value = 1 } }
    local rules = setbox.getRules()

    assert_eq("my-custom-rule", rules[1].id, "rule id")
end

function Tests.test_rule_shorthand_properties()
    print("test_rule_shorthand_properties")
    setbox._clear()

    -- Direct properties without 'apply' wrapper
    setbox.rule { tags = {"Test"}, color = "red", size = 10 }
    local rules = setbox.getRules()

    assert_eq("red", rules[1].properties.color, "shorthand color")
    assert_eq(10, rules[1].properties.size, "shorthand size")
end

function Tests.test_single_tag_syntax()
    print("test_single_tag_syntax")
    setbox._clear()

    setbox.rule { tag = "SingleTag", apply = { value = 99 } }
    setbox.setActiveTags({"SingleTag"})

    assert_eq(99, setbox.getNumber("value"), "single tag match")
end

-- =============================================================================
-- Test: Tag Management
-- =============================================================================

function Tests.test_setActiveTags()
    print("test_setActiveTags")
    setbox._clear()

    setbox.setActiveTags({"A", "B", "C"})
    local tags = setbox.getActiveTags()

    -- Check all tags present (order may vary)
    local tagSet = {}
    for _, t in ipairs(tags) do tagSet[t] = true end

    assert_true(tagSet["A"], "tag A present")
    assert_true(tagSet["B"], "tag B present")
    assert_true(tagSet["C"], "tag C present")
    assert_eq(3, #tags, "three tags total")
end

function Tests.test_addTag()
    print("test_addTag")
    setbox._clear()

    setbox.addTag("First")
    setbox.addTag("Second")

    assert_true(setbox.hasTag("First"), "First tag present")
    assert_true(setbox.hasTag("Second"), "Second tag present")
    assert_false(setbox.hasTag("Third"), "Third tag not present")
end

function Tests.test_removeTag()
    print("test_removeTag")
    setbox._clear()

    setbox.setActiveTags({"A", "B", "C"})
    setbox.removeTag("B")

    assert_true(setbox.hasTag("A"), "A still present")
    assert_false(setbox.hasTag("B"), "B removed")
    assert_true(setbox.hasTag("C"), "C still present")
end

function Tests.test_toggleTag()
    print("test_toggleTag")
    setbox._clear()

    setbox.addTag("Toggle")
    assert_true(setbox.hasTag("Toggle"), "tag added")

    setbox.toggleTag("Toggle")
    assert_false(setbox.hasTag("Toggle"), "tag removed by toggle")

    setbox.toggleTag("Toggle")
    assert_true(setbox.hasTag("Toggle"), "tag re-added by toggle")
end

function Tests.test_hasTag()
    print("test_hasTag")
    setbox._clear()

    assert_false(setbox.hasTag("Missing"), "tag not present")

    setbox.addTag("Present")
    assert_true(setbox.hasTag("Present"), "tag present")
end

-- =============================================================================
-- Test: Base Defaults (C++ Test 1)
-- =============================================================================

function Tests.test_no_tags_base_defaults()
    print("test_no_tags_base_defaults")
    setupTestConfig()
    setbox.setActiveTags({})

    assert_eq("#1a1a2e", setbox.getString("background"), "base background")
    assert_eq("#ffffff", setbox.getString("foreground"), "base foreground")
    assert_eq(14, setbox.getNumber("fontSize"), "base fontSize")
    assert_eq(4, setbox.getNumber("borderRadius"), "base borderRadius")
end

-- =============================================================================
-- Test: Button Tag (C++ Test 2)
-- =============================================================================

function Tests.test_button_tag()
    print("test_button_tag")
    setupTestConfig()
    setbox.setActiveTags({"Button"})

    assert_eq("#3b82f6", setbox.getString("background"), "button background")
    assert_eq("#ffffff", setbox.getString("foreground"), "button foreground")
    assert_eq(8, setbox.getNumber("padding"), "button padding")
    assert_eq(6, setbox.getNumber("borderRadius"), "button borderRadius")
end

-- =============================================================================
-- Test: Primary Button (C++ Test 3)
-- =============================================================================

function Tests.test_primary_button()
    print("test_primary_button")
    setupTestConfig()
    setbox.setActiveTags({"Button", "Primary"})

    -- Primary variant overrides background
    assert_eq("#2563eb", setbox.getString("background"), "primary background")
    assert_eq("bold", setbox.getString("fontWeight"), "primary fontWeight")
    -- Inherits from Button
    assert_eq(8, setbox.getNumber("padding"), "inherited padding")
end

-- =============================================================================
-- Test: Primary Button in Sidebar (C++ Test 4)
-- =============================================================================

function Tests.test_primary_button_in_sidebar()
    print("test_primary_button_in_sidebar")
    setupTestConfig()
    setbox.setActiveTags({"Button", "Primary", "Sidebar"})

    -- Most specific rule (3 tags) wins for background
    assert_eq("#1d4ed8", setbox.getString("background"), "sidebar primary background")
    -- Button+Sidebar rule wins for borderRadius
    assert_eq(0, setbox.getNumber("borderRadius"), "sidebar borderRadius")
    assert_eq("100%", setbox.getString("width"), "sidebar button width")
end

-- =============================================================================
-- Test: Experimental Override (C++ Test 5)
-- =============================================================================

function Tests.test_experimental_override()
    print("test_experimental_override")
    setupTestConfig()
    -- Note: Lua implementation uses specificity > priority > order
    -- So experimental (2 tags, priority 100) needs same or less specificity
    -- to override. Use just {Button, experimental} to test priority override.
    setbox.setActiveTags({"Button", "experimental"})

    -- High priority (100) override wins over regular Button rule
    assert_eq("#dc2626", setbox.getString("background"), "experimental background")
    assert_eq(20, setbox.getNumber("borderRadius"), "experimental borderRadius")
end

function Tests.test_specificity_beats_priority()
    print("test_specificity_beats_priority")
    setupTestConfig()
    -- More specific rule (3 tags) beats high-priority rule (2 tags)
    setbox.setActiveTags({"Button", "Primary", "Sidebar", "experimental"})

    -- Button+Primary+Sidebar (3 tags) beats Button+experimental (2 tags, priority 100)
    assert_eq("#1d4ed8", setbox.getString("background"), "specificity wins")
end

-- =============================================================================
-- Test: Hover State with Context (C++ Test 6)
-- =============================================================================

function Tests.test_hover_state()
    print("test_hover_state")
    setupTestConfig()
    setbox.setActiveTags({"Button"})

    -- Without hover context
    assert_eq("#3b82f6", setbox.getString("background"), "unhovered background")

    -- With hover context
    setbox.setContext("hovered", true)
    assert_eq("#60a5fa", setbox.getString("background"), "hovered background")
    assert_eq("pointer", setbox.getString("cursor"), "hovered cursor")

    -- Clear context
    setbox.clearContext()
    assert_eq("#3b82f6", setbox.getString("background"), "context cleared background")
end

-- =============================================================================
-- Test: Radio Config - 20m CW (C++ Test 7)
-- =============================================================================

function Tests.test_radio_20m_cw()
    print("test_radio_20m_cw")
    setupTestConfig()
    setbox.setActiveTags({"Radio", "20m", "CW"})

    assert_close(14.035e6, setbox.getNumber("frequency"), 1, "20m CW frequency")
    assert_eq("CW", setbox.getString("mode"), "20m CW mode")
    assert_eq(500, setbox.getNumber("filterWidth"), "20m CW filterWidth")
    assert_eq("beam", setbox.getString("antenna"), "20m antenna")
end

-- =============================================================================
-- Test: Radio Config - 40m SSB (C++ Test 8)
-- =============================================================================

function Tests.test_radio_40m_ssb()
    print("test_radio_40m_ssb")
    setupTestConfig()
    setbox.setActiveTags({"Radio", "40m", "SSB"})

    assert_close(7.2e6, setbox.getNumber("frequency"), 1, "40m SSB frequency")
    assert_eq("LSB", setbox.getString("mode"), "40m SSB mode")
    assert_eq(2400, setbox.getNumber("filterWidth"), "40m SSB filterWidth")
end

-- =============================================================================
-- Test: Direct Property Access (C++ Test 9)
-- =============================================================================

function Tests.test_direct_property_access()
    print("test_direct_property_access")
    setupTestConfig()
    setbox.setActiveTags({"Radio", "40m", "SSB"})

    -- Using typed getters
    assert_close(7.2e6, setbox.getNumber("frequency", 0), 1, "getNumber frequency")
    assert_eq("LSB", setbox.getString("mode", "unknown"), "getString mode")
    assert_eq(2400, setbox.getNumber("filterWidth", 0), "getNumber filterWidth")

    -- Using raw get
    local freq = setbox.get("frequency")
    assert_true(type(freq) == "number", "get returns number")
end

-- =============================================================================
-- Test: Property Type Handling
-- =============================================================================

function Tests.test_property_types()
    print("test_property_types")
    setbox._clear()

    setbox.rule {
        tags = {"Test"},
        apply = {
            strVal = "hello",
            numVal = 42.5,
            boolVal = true,
            boolFalse = false,
        }
    }
    setbox.setActiveTags({"Test"})

    assert_eq("hello", setbox.getString("strVal"), "string value")
    assert_eq(42.5, setbox.getNumber("numVal"), "number value")
    assert_eq(true, setbox.getBool("boolVal"), "bool true value")
    assert_eq(false, setbox.getBool("boolFalse"), "bool false value")
end

function Tests.test_default_values()
    print("test_default_values")
    setbox._clear()
    setbox.setActiveTags({})

    -- setbox.has() replaces legacy default parameters
    assert_false(setbox.has("missing"), "has() returns false for missing property")

    -- Getting a missing property must now throw an error
    local ok, err = pcall(function() setbox.get("missing") end)
    assert_false(ok, "get() throws error for missing property")
end

function Tests.test_type_mismatch_throws()
    print("test_type_mismatch_throws")
    setbox._clear()

    setbox.rule {
        tags = {"Test"},
        apply = { value = "not a number" }
    }
    setbox.setActiveTags({"Test"})

    -- Requesting number for string value should throw
    local ok, err = pcall(function() setbox.getNumber("value") end)
    assert_false(ok, "getNumber() throws error for type mismatch")
end

-- =============================================================================
-- Test: Specificity (More Tags Wins)
-- =============================================================================

function Tests.test_specificity_more_tags_wins()
    print("test_specificity_more_tags_wins")
    setbox._clear()

    -- Less specific (1 tag)
    setbox.rule { tags = {"A"}, apply = { value = 1 } }
    -- More specific (2 tags)
    setbox.rule { tags = {"A", "B"}, apply = { value = 2 } }

    setbox.setActiveTags({"A", "B"})
    assert_eq(2, setbox.getNumber("value"), "more specific wins")
end

-- =============================================================================
-- Test: Priority Ordering
-- =============================================================================

function Tests.test_priority_higher_wins()
    print("test_priority_higher_wins")
    setbox._clear()

    setbox.rule { tags = {"A"}, priority = 0, apply = { value = 1 } }
    setbox.rule { tags = {"A"}, priority = 10, apply = { value = 2 } }

    setbox.setActiveTags({"A"})
    assert_eq(2, setbox.getNumber("value"), "higher priority wins")
end

function Tests.test_priority_negative()
    print("test_priority_negative")
    setbox._clear()

    setbox.rule { tags = {"A"}, priority = 0, apply = { value = 1 } }
    setbox.rule { tags = {"A"}, priority = -10, apply = { value = 2 } }

    setbox.setActiveTags({"A"})
    assert_eq(1, setbox.getNumber("value"), "priority 0 beats -10")
end

-- =============================================================================
-- Test: Declaration Order (Later Wins on Tie)
-- =============================================================================

function Tests.test_declaration_order_later_wins()
    print("test_declaration_order_later_wins")
    setbox._clear()

    -- Same tags, same priority
    setbox.rule { tags = {"A"}, apply = { value = 1 } }
    setbox.rule { tags = {"A"}, apply = { value = 2 } }

    setbox.setActiveTags({"A"})
    assert_eq(2, setbox.getNumber("value"), "later declaration wins")
end

-- =============================================================================
-- Test: Rule Enabled/Disabled
-- =============================================================================

function Tests.test_disabled_rule()
    print("test_disabled_rule")
    setbox._clear()

    setbox.rule { tags = {"A"}, apply = { value = 1 } }
    setbox.rule { tags = {"A"}, enabled = false, apply = { value = 2 } }

    setbox.setActiveTags({"A"})
    assert_eq(1, setbox.getNumber("value"), "disabled rule ignored")
end

-- =============================================================================
-- Test: Matching Rules Inspection
-- =============================================================================

function Tests.test_getMatchingRules()
    print("test_getMatchingRules")
    setupTestConfig()
    setbox.setActiveTags({"Button", "Primary"})

    local matching = setbox.getMatchingRules()

    -- Should have: base default, Button, Button+Primary
    assert_true(#matching >= 3, "at least 3 matching rules")

    -- Most specific should be first
    local first = matching[1]
    local tagCount = 0
    for _ in pairs(first.tags) do tagCount = tagCount + 1 end
    assert_eq(2, tagCount, "most specific rule has 2 tags")
end

-- =============================================================================
-- Test: Change Callbacks
-- =============================================================================

function Tests.test_change_callback()
    print("test_change_callback")
    setbox._clear()

    local changedProps = {}
    setbox.onPropertyChange(function(name, value)
        changedProps[name] = value
    end)

    setbox.rule { tags = {"A"}, apply = { prop1 = "value1" } }
    setbox.setActiveTags({"A"})

    assert_eq("value1", changedProps["prop1"], "callback received change")
end

function Tests.test_callback_on_tag_change()
    print("test_callback_on_tag_change")
    setbox._clear()

    local callCount = 0
    setbox.onPropertyChange(function(name, value)
        callCount = callCount + 1
    end)

    setbox.rule { tags = {"A"}, apply = { x = 1 } }
    setbox.rule { tags = {"B"}, apply = { y = 2 } }

    setbox.setActiveTags({"A"})
    local countAfterA = callCount

    setbox.setActiveTags({"B"})
    assert_true(callCount > countAfterA, "callback fired on tag change")
end

-- =============================================================================
-- Test: Context Conditions
-- =============================================================================

function Tests.test_context_condition_false()
    print("test_context_condition_false")
    setbox._clear()

    setbox.rule {
        tags = {"A"},
        ["when"] = function(ctx) return ctx.enabled == true end,
        apply = { value = 100 }
    }
    setbox.rule { tags = {"A"}, apply = { value = 50 } }

    setbox.setActiveTags({"A"})

    -- Condition is false (enabled not set), so fallback rule wins
    assert_eq(50, setbox.getNumber("value"), "condition false, fallback")
end

function Tests.test_context_condition_true()
    print("test_context_condition_true")
    setbox._clear()

    setbox.rule {
        tags = {"A"},
        ["when"] = function(ctx) return ctx.enabled == true end,
        priority = 10,
        apply = { value = 100 }
    }
    setbox.rule { tags = {"A"}, apply = { value = 50 } }

    setbox.setActiveTags({"A"})
    setbox.setContext("enabled", true)

    assert_eq(100, setbox.getNumber("value"), "condition true, high priority")
end

-- =============================================================================
-- Test: Property Merging
-- =============================================================================

function Tests.test_property_merge()
    print("test_property_merge")
    setbox._clear()

    setbox.rule { tags = {"A"}, apply = { x = 1, y = 2 } }
    setbox.rule { tags = {"A", "B"}, apply = { y = 20, z = 30 } }

    setbox.setActiveTags({"A", "B"})

    -- x from {A}, y overridden by {A,B}, z from {A,B}
    assert_eq(1, setbox.getNumber("x"), "inherited x")
    assert_eq(20, setbox.getNumber("y"), "overridden y")
    assert_eq(30, setbox.getNumber("z"), "new z")
end

-- =============================================================================
-- Test: Global Rule (Empty Tags)
-- =============================================================================

function Tests.test_global_rule_empty_tags()
    print("test_global_rule_empty_tags")
    setbox._clear()

    setbox.rule { tags = {}, apply = { globalProp = "always" } }
    setbox.rule { tags = {"X"}, apply = { otherProp = "onlyX" } }

    -- With no tags, global rule applies
    setbox.setActiveTags({})
    assert_eq("always", setbox.getString("globalProp"), "global with no tags")
    assert_false(setbox.has("otherProp"), "X rule not matched")

    -- With X tag, both apply
    setbox.setActiveTags({"X"})
    assert_eq("always", setbox.getString("globalProp"), "global with tags")
    assert_eq("onlyX", setbox.getString("otherProp"), "X rule matched")
end

-- =============================================================================
-- Test: Resolve Caching
-- =============================================================================

function Tests.test_resolve_caching()
    print("test_resolve_caching")
    setbox._clear()

    setbox.rule { tags = {"A"}, apply = { value = 1 } }
    setbox.setActiveTags({"A"})

    -- First resolve
    local props1 = setbox.resolve()

    -- Second resolve (should be cached)
    local props2 = setbox.resolve()

    -- Should be the exact same table (caching)
    assert_true(props1 == props2, "cached resolve returns same table")
end

function Tests.test_cache_invalidation()
    print("test_cache_invalidation")
    setbox._clear()

    setbox.rule { tags = {"A"}, apply = { value = 1 } }
    setbox.rule { tags = {"B"}, apply = { value = 2 } }

    setbox.setActiveTags({"A"})
    local props1 = setbox.resolve()

    setbox.setActiveTags({"B"})
    local props2 = setbox.resolve()

    assert_eq(1, props1.value, "first resolve value")
    assert_eq(2, props2.value, "second resolve after invalidation")
end

-- =============================================================================
-- Test: Clear Function
-- =============================================================================

function Tests.test_clear()
    print("test_clear")
    setbox._clear()

    setbox.rule { tags = {"A"}, apply = { value = 1 } }
    setbox.addTag("A")
    setbox.setContext("x", 1)

    setbox._clear()

    assert_eq(0, #setbox.getRules(), "rules cleared")
    assert_eq(0, #setbox.getActiveTags(), "tags cleared")
    assert_false(setbox.has("value"), "properties cleared")
end

-- =============================================================================
-- Test Runner
-- =============================================================================

function Tests.runAll()
    print("\n--- SetBox Tests ---")
    passed = 0
    failed = 0

    local tests = {}
    for name, fn in pairs(Tests) do
        if type(fn) == "function" and name:match("^test_") then
            table.insert(tests, {name = name, fn = fn})
        end
    end

    -- Sort for consistent ordering
    table.sort(tests, function(a, b) return a.name < b.name end)

    for _, test in ipairs(tests) do
        local ok, err = pcall(test.fn)
        if not ok then
            failed = failed + 1
            print(string.format("  ERROR in %s: %s", test.name, err))
        end
    end

    print(string.format("\nSetBox: %d passed, %d failed", passed, failed))
    return failed == 0
end

return Tests
