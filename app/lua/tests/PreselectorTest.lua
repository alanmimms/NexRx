--[[
    Unit tests for Preselector widget SetBox integration
    Verifies Local Widget Context (LWC) nesting and property inheritance.
]]

local setbox = require("SetBox")
local Preselector = require("ui.Preselector")

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

-- Mock UI functions that widgets call
_G.drawRoundedRect = function() end
_G.drawRectOutline = function() end
_G.drawText = function() end
_G.drawRect = function() end
_G.drawLine = function() end
_G.measureText = function() return 50 end
_G.getLineHeight = function() return 16 end

-- Mock modules
package.loaded["ui.Widgets"] = require("ui.Widgets")
package.loaded["ui.Layout"] = require("ui.Layout")
package.loaded["ui.State"] = require("ui.State")

local ui = require("ui.Widgets")
local layout = require("ui.Layout")
local state = require("ui.State")

-- Helper: setup test configuration
local function setupTestConfig()
    setbox._clear()
    
    -- 1. Global defaults
    setbox.rule {
        id = "global-defaults",
        tags = {},
        apply = {
            background = "#000000",
            foreground = "#ffffff",
            border = "#444444",
            accent = "#00ff00",
            borderWidth = 1,
            borderRadius = 4,
            opacity = 1.0,
            padding = 10,
            topMargin = 30,
            gridRowHeight = 20,
            gridRowGap = 4,
            gridColWidth = 60,
            gridColGap = 8,
            gridCols = 4,
            boxSize = 18,
            spacing = 8,
        }
    }

    -- 1b. Label defaults
    setbox.rule {
        id = "label-defaults",
        tags = {"widget.Label"},
        apply = {
            text = "Label",
        }
    }

    -- 2. Preselector specific rules
    setbox.rule {
        id = "presel-frame-style",
        tags = {"widget.PreselectorFrame"},
        apply = {
            background = "#222222", -- Overrides global
            padding = 10,
            title = "PRESELECTOR",
            labelAuto = "Auto",
            labelL1 = "L1",
        }
    }

    -- 3. Nesting test: Checkbox inside Preselector should see a property 
    -- defined for that combination.
    setbox.rule {
        id = "presel-checkbox-special",
        tags = {"widget.PreselectorFrame", "widget.Checkbox"},
        apply = {
            boxSize = 24, -- Bigger boxes in preselector
        }
    }
end

function Tests.test_lwc_nesting()
    print("test_lwc_nesting")
    setupTestConfig()
    
    -- Mock app state
    local appState = {
        preselectorAuto = true,
        preselL1 = false,
    }
    for i = 0, 10 do appState["preselC"..i] = false end

    -- We want to verify that when Preselector:draw is called, 
    -- it creates an LWC and passes it to sub-widgets.
    
    local p = Preselector.new(appState)
    layout.begin(0, 0, 800, 600)
    
    -- We need to intercept setbox.newContext to verify nesting
    local originalNewContext = setbox.newContext
    local capturedContexts = {}
    
    setbox.newContext = function(tags, parent)
        local ctx = originalNewContext(tags, parent)
        table.insert(capturedContexts, { tags = tags, parent = parent, ctx = ctx })
        return ctx
    end
    
    p:draw("presel1", 10, 10, 300, 200)
    
    setbox.newContext = originalNewContext
    
    -- Verify first context is the Preselector frame
    assert_eq(true, capturedContexts[1].tags[2] == "widget.PreselectorFrame", "First LWC is PreselectorFrame")
    local frameLWC = capturedContexts[1].ctx
    
    -- Verify subsequent contexts (labels, checkboxes) have frameLWC as parent
    local foundCheckbox = false
    for i = 2, #capturedContexts do
        assert_eq(frameLWC, capturedContexts[i].parent, "Widget " .. i .. " has Preselector LWC as parent")
        
        -- Check if this is one of our checkboxes
        local isCheckbox = false
        for _, t in ipairs(capturedContexts[i].tags) do
            if t == "widget.Checkbox" then isCheckbox = true end
        end
        
        if isCheckbox then
            foundCheckbox = true
            assert_eq(24, capturedContexts[i].ctx:getNumber("boxSize"), "Checkbox " .. i .. " inherits boxSize=24 from parent context tags")
        end
    end
    
    assert_eq(true, foundCheckbox, "Found at least one checkbox in captured contexts")
    
    layout.finish()
end

function Tests.test_global_tag_inheritance()
    print("test_global_tag_inheritance")
    setupTestConfig()
    
    -- Add a global tag
    setbox.addTag("theme.Dark")
    
    -- Rule that depends on global tag
    setbox.rule {
        tags = {"theme.Dark", "widget.PreselectorFrame"},
        apply = {
            border = "#444444"
        }
    }
    
    local lwc = setbox.newContext({"widget.PreselectorFrame"})
    assert_eq("#444444", lwc:getString("border"), "LWC inherits global tags for resolution")
end

function Tests.runAll()
    print("\n--- Preselector Widget Tests ---")
    passed = 0
    failed = 0

    local tests = {
        {"test_lwc_nesting", Tests.test_lwc_nesting},
        {"test_global_tag_inheritance", Tests.test_global_tag_inheritance},
    }

    for _, t in ipairs(tests) do
        local ok, err = pcall(t[2])
        if not ok then
            failed = failed + 1
            print(string.format("  ERROR in %s: %s", t[1], err))
        end
    end

    print(string.format("\nPreselector: %d passed, %d failed", passed, failed))
    return failed == 0
end

return Tests
