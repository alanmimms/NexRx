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

-- Input and state rule registries (for unified tag architecture)
local inputRules = {}
local stateRules = {}

-- =============================================================================
-- Tag Parsing (supports @N priority suffix)
-- =============================================================================

--- Parse a tag string, extracting name and priority
-- "widget.Button" -> "widget.Button", 1
-- "widget.Button@5" -> "widget.Button", 5
-- @param tagStr The tag string to parse
-- @return name, priority
local function parseTag(tagStr)
    local name, pri = tagStr:match("^(.-)@(%d+)$")
    if name and pri then
        return name, tonumber(pri)
    end
    return tagStr, 1  -- Default priority is 1
end

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
--   tags: List of required tags with optional @N priority suffix
--   tag: Single tag string (alternative to tags)
--   priority: Number (default 0, added to tag priorities for tie-breaking)
--   when: Function(ctx) -> bool for dynamic conditions
--   enabled: Boolean (default true)
--   id: String identifier (auto-generated if missing)
--   apply: Table of property name -> value mappings
--   Direct properties (shorthand for apply = {...})
function SetBox.rule(def)
    local rule = {
        id = def.id or ("rule_" .. tostring(nextRuleOrder)),
        tags = {},           -- tag name -> priority (parsed from @N suffix)
        condition = def["when"],
        priority = def.priority or 0,  -- Base priority added to tag sum
        enabled = def.enabled ~= false,
        properties = {},
        declarationOrder = nextRuleOrder,
    }
    nextRuleOrder = nextRuleOrder + 1

    -- Extract tags with priority parsing
    if def.tags then
        for _, tagStr in ipairs(def.tags) do
            local name, pri = parseTag(tagStr)
            rule.tags[name] = pri
        end
    end
    if def.tag then
        local name, pri = parseTag(def.tag)
        rule.tags[name] = pri
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
-- Score = sum of tag priorities + rule base priority
-- @return 0 if no match, otherwise sum of priorities (min 1)
local function matchScore(rule)
    if not rule.enabled then
        return 0
    end

    -- All rule tags must be present in active tags
    -- Score is sum of tag priorities
    local prioritySum = 0
    local hasAnyTags = false
    for tagName, tagPriority in pairs(rule.tags) do
        hasAnyTags = true
        if not activeTags[tagName] then
            return 0  -- Tag not active, rule doesn't match
        end
        prioritySum = prioritySum + tagPriority
    end

    -- Evaluate condition if present
    if rule.condition then
        local ok, result = pcall(rule.condition, context)
        if not ok or not result then
            return 0
        end
    end

    -- Add rule's base priority
    prioritySum = prioritySum + rule.priority

    -- Return at least 1 for rules with empty tags (global defaults)
    return hasAnyTags and prioritySum or 1
end

--- Compare rules for sorting by score
-- Higher score (sum of tag priorities + base priority) wins
-- Ties broken by declaration order (later wins)
local function compareRules(a, b, scoreA, scoreB)
    -- Higher score wins
    if scoreA ~= scoreB then
        return scoreA > scoreB
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

    -- Collect matching rules with their scores
    local matching = {}
    for _, rule in ipairs(rules) do
        local score = matchScore(rule)
        if score > 0 then
            table.insert(matching, { rule = rule, score = score })
        end
    end

    -- Sort by score (highest first), then declaration order
    table.sort(matching, function(a, b)
        return compareRules(a.rule, b.rule, a.score, b.score)
    end)

    -- Merge properties (later in sorted order = higher score = wins)
    -- So we iterate in reverse to let high-score rules override
    local result = {}
    for i = #matching, 1, -1 do
        for name, value in pairs(matching[i].rule.properties) do
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

--- Get matching rules for current tags (with scores)
function SetBox.getMatchingRules()
    local matching = {}
    for _, rule in ipairs(rules) do
        local score = matchScore(rule)
        if score > 0 then
            table.insert(matching, { rule = rule, score = score })
        end
    end
    table.sort(matching, function(a, b)
        return compareRules(a.rule, b.rule, a.score, b.score)
    end)
    -- Return just rules for API compatibility
    local result = {}
    for _, m in ipairs(matching) do
        table.insert(result, m.rule)
    end
    return result
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
    inputRules = {}
    stateRules = {}
    nextRuleOrder = 0
end

-- =============================================================================
-- Input Rules (SDL event to tag mapping)
-- =============================================================================

--- Register an input mapping rule
-- @param def Input rule definition:
--   sdl: { type = "MOUSEBUTTONDOWN", button = 1, ... }
--   emit: { tag = "event.MouseDown-LEFT", properties = { "x", "y" } }
--   hold: { tag = "input.MouseLEFT" } (persists until release)
--   also: { tag = "input.SHIFT" } (derived tag, added with hold)
function SetBox.inputRule(def)
    table.insert(inputRules, {
        sdl = def.sdl,
        emit = def.emit,
        hold = def.hold,
        also = def.also,
    })
end

--- Get all input rules
function SetBox.getInputRules()
    return inputRules
end

-- =============================================================================
-- State Rules (property value to tag mapping)
-- =============================================================================

--- Register a state-to-tag mapping rule
-- @param def State rule definition:
--   when: { property = "currentMode", equals = "USB" }
--   activate: { "state.Mode-USB", "state.Mode-SSB" }
function SetBox.stateRule(def)
    table.insert(stateRules, {
        when = def.when,
        activate = def.activate,
    })
end

--- Evaluate all state rules against a state table
-- @param stateTable Table of property name -> value
-- @return Table of tag name -> true for all activated tags
function SetBox.evaluateStateRules(stateTable)
    local stateTags = {}
    for _, rule in ipairs(stateRules) do
        local prop = rule.when.property
        local expected = rule.when.equals
        local actual = stateTable[prop]
        if actual == expected then
            for _, tag in ipairs(rule.activate) do
                stateTags[tag] = true
            end
        end
    end
    return stateTags
end

--- Get all state rules
function SetBox.getStateRules()
    return stateRules
end

-- =============================================================================
-- Global Exports
-- =============================================================================

-- Make rule(), inputRule(), stateRule() available globally so config files can use them
_G.rule = function(def)
    SetBox.rule(def)
end

_G.inputRule = function(def)
    SetBox.inputRule(def)
end

_G.stateRule = function(def)
    SetBox.stateRule(def)
end

-- Make setbox available globally (for C++ verification and direct access)
_G.setbox = SetBox

return SetBox
