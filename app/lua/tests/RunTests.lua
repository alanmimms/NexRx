#!/usr/bin/env lua
--[[
    Test Runner for NexRx Lua Modules

    Runs all test modules and reports results.

    Usage:
        lua tests/RunTests.lua
        -- or from app/lua directory:
        lua tests/RunTests.lua

    Individual test modules:
        lua -e "package.path='?.lua;?/init.lua;'..package.path; require('tests.LayoutTest').runAll()"
]]

-- Set up package path to find modules
local scriptPath = arg[0]:match("(.*/)")
if scriptPath then
    package.path = scriptPath .. "../?.lua;" ..
                   scriptPath .. "../?/init.lua;" ..
                   scriptPath .. "?.lua;" ..
                   package.path
else
    package.path = "lua/?.lua;lua/?/init.lua;?.lua;?/init.lua;" .. package.path
end

print("========================================")
print("  NexRx Lua Module Test Suite")
print("========================================")

local allPassed = true
local modules = {}

-- Try to load and run each test module
local testModules = {
    {"LayoutTest", "Layout System"},
    {"EventsTest", "Event Dispatch"},
    {"AnimateTest", "Animation System"},
    {"BandsTest", "Band Detection"},
    {"SetBoxTest", "SetBox Engine"},
    {"PreselectorTest", "Preselector Widget"},
}

for _, info in ipairs(testModules) do
    local moduleName, description = info[1], info[2]
    print(string.format("\nLoading %s tests...", description))

    local ok, testModule = pcall(require, "tests." .. moduleName)
    if ok then
        local testOk, result = pcall(testModule.runAll)
        if testOk then
            if not result then
                allPassed = false
                table.insert(modules, {name = description, status = "FAILED"})
            else
                table.insert(modules, {name = description, status = "PASSED"})
            end
        else
            allPassed = false
            table.insert(modules, {name = description, status = "ERROR: " .. tostring(result)})
            print("  ERROR running tests: " .. tostring(result))
        end
    else
        print("  Could not load module: " .. tostring(testModule))
        -- Try alternate path
        ok, testModule = pcall(require, moduleName)
        if ok then
            local testOk, result = pcall(testModule.runAll)
            if testOk then
                if not result then
                    allPassed = false
                    table.insert(modules, {name = description, status = "FAILED"})
                else
                    table.insert(modules, {name = description, status = "PASSED"})
                end
            else
                allPassed = false
                table.insert(modules, {name = description, status = "ERROR"})
            end
        else
            table.insert(modules, {name = description, status = "SKIPPED (not found)"})
        end
    end
end

-- Summary
print("\n========================================")
print("  Test Summary")
print("========================================")

for _, mod in ipairs(modules) do
    local icon = mod.status == "PASSED" and "[OK]" or "[!!]"
    print(string.format("  %s %s: %s", icon, mod.name, mod.status))
end

print("========================================")
if allPassed then
    print("  ALL TESTS PASSED")
else
    print("  SOME TESTS FAILED")
end
print("========================================\n")

-- Return exit code
os.exit(allPassed and 0 or 1)
