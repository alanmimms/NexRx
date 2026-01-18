--[[
  SetBox - Pure Lua Configuration Engine

  A tag-based property resolution system. Rules map tag combinations
  to property values. Resolution follows:
    1. Specificity (more matching tags wins)
    2. Priority (higher wins)
    3. Declaration order (later wins)

  Usage:
    local setbox = require("setbox")

    setbox.rule {
        tags = {"Button", "Primary"},
        priority = 10,
        apply = {
            color = "#3b82f6",
            borderRadius = 4,
        }
    }

    setbox.addTag("Button")
    setbox.addTag("Primary")

    local color = setbox.getString("color", "#000000")
]]

local SetBox = {}

-- Internal state
local rules = {}
local activeTags = {}
local context = {}
local cachedProperties = {}
local cacheValid = false
local changeCallbacks = {}
local nativeCallbacks = {}  -- Callbacks from C++ (registered via _registerNativeCallback)
local nextRuleOrder = 0

-- =============================================================================
-- Tag Management
-- =============================================================================

--- Set the complete active tag set
-- @param tags Table of tag strings
function SetBox.setActiveTags(tags)
    local newTags = {}
    for _, tag in ipairs(tags) do
        newTags[tag] = true
    end

    -- Check if changed
    local changed = false
    for tag in pairs(newTags) do
        if not activeTags[tag] then changed = true; break end
    end
    if not changed then
        for tag in pairs(activeTags) do
            if not newTags[tag] then changed = true; break end
        end
    end

    if changed then
        activeTags = newTags
        SetBox._invalidateCache()
    end
end

--- Add a tag to the active set
function SetBox.addTag(tag)
    if not activeTags[tag] then
        activeTags[tag] = true
        SetBox._invalidateCache()
    end
end

--- Remove a tag from the active set
function SetBox.removeTag(tag)
    if activeTags[tag] then
        activeTags[tag] = nil
        SetBox._invalidateCache()
    end
end

--- Toggle a tag (add if absent, remove if present)
function SetBox.toggleTag(tag)
    if activeTags[tag] then
        activeTags[tag] = nil
    else
        activeTags[tag] = true
    end
    SetBox._invalidateCache()
end

--- Check if a tag is active
function SetBox.hasTag(tag)
    return activeTags[tag] == true
end

--- Get all active tags as a list
function SetBox.getActiveTags()
    local result = {}
    for tag in pairs(activeTags) do
        table.insert(result, tag)
    end
    return result
end

-- =============================================================================
-- Context Management
-- =============================================================================

--- Set a context value for condition evaluation
function SetBox.setContext(name, value)
    if context[name] ~= value then
        context[name] = value
        SetBox._invalidateCache()
    end
end

--- Clear all context values
function SetBox.clearContext()
    context = {}
    SetBox._invalidateCache()
end

-- =============================================================================
-- Rule Registration
-- =============================================================================

--- Register a rule
-- @param def Rule definition table with:
--   tags: List of required tags (optional, empty = global default)
--   tag: Single tag string (alternative to tags)
--   priority: Number (default 0, higher wins)
--   when: Function(ctx) -> bool for dynamic conditions
--   enabled: Boolean (default true)
--   id: String identifier (auto-generated if missing)
--   apply: Table of property name -> value mappings
--   Direct properties (shorthand for apply = {...})
function SetBox.rule(def)
    local rule = {
        id = def.id or ("rule_" .. tostring(nextRuleOrder)),
        tags = {},
        condition = def["when"],
        priority = def.priority or 0,
        enabled = def.enabled ~= false,
        properties = {},
        declarationOrder = nextRuleOrder,
    }
    nextRuleOrder = nextRuleOrder + 1

    -- Extract tags
    if def.tags then
        for _, tag in ipairs(def.tags) do
            rule.tags[tag] = true
        end
    end
    if def.tag then
        rule.tags[def.tag] = true
    end

    -- Extract properties from 'apply' table
    if def.apply then
        for name, value in pairs(def.apply) do
            rule.properties[name] = value
        end
    end

    -- Extract direct properties (shorthand syntax)
    local reserved = {
        tags = true, tag = true, ["when"] = true,
        priority = true, enabled = true, id = true, apply = true
    }
    for name, value in pairs(def) do
        if not reserved[name] then
            rule.properties[name] = value
        end
    end

    table.insert(rules, rule)
    SetBox._invalidateCache()
end

-- =============================================================================
-- Rule Matching
-- =============================================================================

--- Calculate match score for a rule
-- @return 0 if no match, otherwise number of matching tags (min 1)
local function matchScore(rule)
    if not rule.enabled then
        return 0
    end

    -- All rule tags must be present in active tags
    local tagCount = 0
    for tag in pairs(rule.tags) do
        tagCount = tagCount + 1
        if not activeTags[tag] then
            return 0
        end
    end

    -- Evaluate condition if present
    if rule.condition then
        local ok, result = pcall(rule.condition, context)
        if not ok or not result then
            return 0
        end
    end

    -- Return at least 1 for rules with empty tags (global defaults)
    return tagCount > 0 and tagCount or 1
end

--- Compare rules for sorting by specificity
-- More specific (more tags) > higher priority > later declaration
local function compareRules(a, b)
    local aTagCount = 0
    for _ in pairs(a.tags) do aTagCount = aTagCount + 1 end
    local bTagCount = 0
    for _ in pairs(b.tags) do bTagCount = bTagCount + 1 end

    -- More tags = more specific = wins
    if aTagCount ~= bTagCount then
        return aTagCount > bTagCount
    end
    -- Higher priority wins
    if a.priority ~= b.priority then
        return a.priority > b.priority
    end
    -- Later declaration wins
    return a.declarationOrder > b.declarationOrder
end

-- =============================================================================
-- Property Resolution
-- =============================================================================

--- Resolve and return all properties for current tags/context
function SetBox.resolve()
    if cacheValid then
        return cachedProperties
    end

    -- Collect matching rules
    local matching = {}
    for _, rule in ipairs(rules) do
        if matchScore(rule) > 0 then
            table.insert(matching, rule)
        end
    end

    -- Sort by specificity (most specific first)
    table.sort(matching, compareRules)

    -- Merge properties (later in sorted order = higher priority = wins)
    -- So we iterate in reverse to let high-priority rules override
    local result = {}
    for i = #matching, 1, -1 do
        for name, value in pairs(matching[i].properties) do
            result[name] = value
        end
    end

    cachedProperties = result
    cacheValid = true

    return result
end

--- Get a specific property value
function SetBox.get(name)
    local props = SetBox.resolve()
    return props[name]
end

--- Get a property as a number
function SetBox.getNumber(name, defaultVal)
    local val = SetBox.get(name)
    if type(val) == "number" then
        return val
    end
    return defaultVal
end

--- Get a property as a string
function SetBox.getString(name, defaultVal)
    local val = SetBox.get(name)
    if type(val) == "string" then
        return val
    end
    return defaultVal
end

--- Get a property as a boolean
function SetBox.getBool(name, defaultVal)
    local val = SetBox.get(name)
    if type(val) == "boolean" then
        return val
    end
    return defaultVal
end

-- =============================================================================
-- Change Notification
-- =============================================================================

--- Register a callback for property changes
function SetBox.onPropertyChange(callback)
    table.insert(changeCallbacks, callback)
end

--- Internal: Invalidate cache and notify changes
function SetBox._invalidateCache()
    local oldProps = cachedProperties
    cacheValid = false

    -- Re-resolve
    local newProps = SetBox.resolve()

    -- Notify changes
    if #changeCallbacks > 0 then
        SetBox._notifyChanges(oldProps, newProps)
    end
end

--- Internal: Notify property change callbacks
function SetBox._notifyChanges(oldProps, newProps)
    -- Find changed properties
    for name, value in pairs(newProps) do
        local oldVal = oldProps[name]
        if oldVal ~= value then
            -- Lua callbacks
            for _, callback in ipairs(changeCallbacks) do
                local ok, err = pcall(callback, name, value)
                if not ok then
                    print("[SetBox] Callback error for " .. name .. ": " .. tostring(err))
                end
            end
            -- Native (C++) callbacks
            for _, callback in ipairs(nativeCallbacks) do
                local ok, err = pcall(callback, name, value)
                if not ok then
                    print("[SetBox] Native callback error for " .. name .. ": " .. tostring(err))
                end
            end
        end
    end
end

--- Register a native (C++) callback for property changes
-- This is called from C++ to bridge property changes back to DSP
function SetBox._registerNativeCallback(callback)
    table.insert(nativeCallbacks, callback)
end

-- =============================================================================
-- File Loading
-- =============================================================================

--- Load and execute a Lua configuration file
-- The file can use rule() to register rules
function SetBox.loadFile(path)
    local chunk, err = loadfile(path)
    if not chunk then
        print("[SetBox] Failed to load " .. path .. ": " .. tostring(err))
        return false
    end

    local ok, err2 = pcall(chunk)
    if not ok then
        print("[SetBox] Error executing " .. path .. ": " .. tostring(err2))
        return false
    end

    return true
end

-- =============================================================================
-- Inspection / Debug
-- =============================================================================

--- Get all registered rules
function SetBox.getRules()
    return rules
end

--- Get matching rules for current tags
function SetBox.getMatchingRules()
    local matching = {}
    for _, rule in ipairs(rules) do
        if matchScore(rule) > 0 then
            table.insert(matching, rule)
        end
    end
    table.sort(matching, compareRules)
    return matching
end

--- Clear all rules (for testing)
function SetBox._clear()
    rules = {}
    activeTags = {}
    context = {}
    cachedProperties = {}
    cacheValid = false
    changeCallbacks = {}
    nativeCallbacks = {}
    nextRuleOrder = 0
end

-- =============================================================================
-- Global Exports
-- =============================================================================

-- Make rule() available globally so config files can use it
_G.rule = function(def)
    SetBox.rule(def)
end

-- Make setbox available globally (for C++ verification and direct access)
_G.setbox = SetBox

return SetBox
