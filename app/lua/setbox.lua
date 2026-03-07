--[[
  SetBox - Rule-based Configuration and Styling System
  
  SetBox manages a collection of rules that apply properties based on 
  active "tags" and context. It is used for everything from hardware 
  defaults to UI styling and localization.
]]

local SetBox = {}
_G.setbox = SetBox
_G.rule = function(def) return SetBox.rule(def) end

-- Internal state
local rules = {}
local activeTags = {} -- Global tags
local globalContext = {} -- Global context values
local nextRuleOrder = 1
local changeCallbacks = {}

-- Cache for resolved properties
local cachedProperties = {}
local cacheValid = false

-- =============================================================================
-- Internal Helpers
-- =============================================================================

local function _invalidateCache(silent)
    cacheValid = false
    if not globalCtx then return end
    
    local oldProps = cachedProperties
    cachedProperties = globalCtx:resolve()
    cacheValid = true

    if silent then return end

    -- Notify callbacks of all properties (simplistic for tests)
    for _, cb in ipairs(changeCallbacks) do
        for name, value in pairs(cachedProperties) do
            if value ~= oldProps[name] then
                pcall(cb, name, value)
            end
        end
    end
end

function SetBox._clear()
    rules = {}
    activeTags = {}
    globalContext = {}
    nextRuleOrder = 1
    changeCallbacks = {}
    _invalidateCache()
end

--- Parse a tag string like "tag@10" into name and priority
local function parseTag(tagStr)
    if not tagStr then return "nil", 0 end
    local name, pri = tagStr:match("([^@]+)@?(%d*)")
    return name, tonumber(pri) or 0
end

-- =============================================================================
-- Context Object (LWC - Local Widget Context)
-- =============================================================================

local Context = {}
Context.__index = Context

local globalCtx = setmetatable({localTags = {}}, Context)

function Context.new(tags, parent)
    local self = setmetatable({}, Context)
    self.localTags = {}
    if tags then
        for _, t in ipairs(tags) do
            local name, _ = parseTag(t)
            self.localTags[name] = true
        end
    end
    self.parent = parent
    return self
end

--- Check if a tag is active in this context (local or parent or global)
function Context:hasTag(tag)
    if self.localTags[tag] then return true end
    if self.parent then return self.parent:hasTag(tag) end
    return activeTags[tag] == true
end

--- Get all tags contributing to this context (for error reporting)
function Context:getHierarchyTags()
    local tags = {}
    -- Global tags first
    for t in pairs(activeTags) do tags[t] = true end
    -- Parent tags
    if self.parent then
        local pTags = self.parent:getHierarchyTags()
        for t in pairs(pTags) do tags[t] = true end
    end
    -- Local tags last (overrides)
    for t in pairs(self.localTags) do tags[t] = true end
    
    local list = {}
    for t in pairs(tags) do table.insert(list, t) end
    return list
end

--- Resolve properties for this specific context
function Context:resolve()
    local matching = self:getMatchingRules()
    
    -- Merge properties (higher score wins)
    local result = {}
    for i = #matching, 1, -1 do
        for name, value in pairs(matching[i].rule.properties) do
            result[name] = value
        end
    end
    return result
end

function Context:getMatchingRules()
    local matching = {}
    for _, rule in ipairs(rules) do
        if rule.enabled then
            local matches = true
            local prioritySum = rule.priority
            local tagCount = 0
            
            for tag, pri in pairs(rule.tags) do
                if not self:hasTag(tag) then
                    matches = false
                    break
                end
                prioritySum = prioritySum + pri
                tagCount = tagCount + 1
            end
            
            if matches and rule.condition then
                if not rule.condition(globalContext) then matches = false end
            end
            
            if matches then
                table.insert(matching, { 
                    rule = rule, 
                    score = prioritySum, 
                    specificity = tagCount,
                    tags = rule.tags,
                    declarationOrder = rule.declarationOrder 
                })
            end
        end
    end

    -- Sort by specificity (highest first), then score (priority), then declaration order
    table.sort(matching, function(a, b)
        if a.specificity ~= b.specificity then return a.specificity > b.specificity end
        if a.score ~= b.score then return a.score > b.score end
        return a.declarationOrder > b.declarationOrder
    end)
    
    return matching
end

function Context:get(name)
    local props = self:resolve()
    local val = props[name]
    if val == nil then
        local tagList = table.concat(self:getHierarchyTags(), ", ")
        error(string.format("[SetBox] Property '%s' not found in any matching rule.\nContext Tags: [%s]", name, tagList), 2)
    end
    return val
end

function Context:has(name)
    local props = self:resolve()
    return props[name] ~= nil
end

function Context:getNumber(name)
    local val = self:get(name)
    if type(val) == "number" then return val end
    local num = tonumber(val)
    if not num then
        error(string.format("[SetBox] Property '%s' expected number, got %s", name, type(val)), 2)
    end
    return num
end

function Context:getString(name)
    local val = self:get(name)
    return tostring(val)
end

function Context:getBool(name)
    local val = self:get(name)
    if type(val) == "boolean" then return val end
    return val == true or val == "true" or val == 1 or val == "1" or val == "yes"
end

-- =============================================================================
-- Tag Management (Global)
-- =============================================================================

function SetBox.addTag(tag)
    local name, _ = parseTag(tag)
    if not activeTags[name] then
        activeTags[name] = true
        _invalidateCache(true)
    end
end

function SetBox.removeTag(tag)
    local name, _ = parseTag(tag)
    if activeTags[name] then
        activeTags[name] = nil
        _invalidateCache(true)
    end
end

function SetBox.setActiveTags(tags)
    local newTags = {}
    for _, tag in ipairs(tags) do
        local name, _ = parseTag(tag)
        newTags[name] = true
    end
    activeTags = newTags
    _invalidateCache(true)
end

function SetBox.getActiveTags()
    local result = {}
    for tag in pairs(activeTags) do table.insert(result, tag) end
    return result
end

function SetBox.hasTag(tag)
    return activeTags[tag] == true
end

function SetBox.toggleTag(tag)
    if SetBox.hasTag(tag) then
        SetBox.removeTag(tag)
    else
        SetBox.addTag(tag)
    end
end

function SetBox.setContext(name, value)
    globalContext[name] = value
    _invalidateCache()
end

function SetBox.clearContext()
    globalContext = {}
    _invalidateCache()
end

function SetBox.onPropertyChange(callback)
    table.insert(changeCallbacks, callback)
end

-- =============================================================================
-- Rule Registration
-- =============================================================================

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

    if def.apply then
        for name, value in pairs(def.apply) do rule.properties[name] = value end
    end

    -- Direct properties shorthand
    local reserved = {id=1, tags=1, tag=1, priority=1, enabled=1, apply=1, ["when"]=1}
    for k, v in pairs(def) do
        if not reserved[k] then rule.properties[k] = v end
    end

    table.insert(rules, rule)
    _invalidateCache()
    return rule
end

function SetBox.getRules()
    return rules
end

-- =============================================================================
-- Property Resolution (Global / Legacy)
-- =============================================================================

function SetBox.resolve()
    if cacheValid then return cachedProperties end
    cachedProperties = globalCtx:resolve()
    cacheValid = true
    return cachedProperties
end

function SetBox.getMatchingRules()
    return globalCtx:getMatchingRules()
end

function SetBox.has(name)
    return globalCtx:has(name)
end

function SetBox.get(name)
    return globalCtx:get(name)
end

function SetBox.getNumber(name)
    return globalCtx:getNumber(name)
end

function SetBox.getString(name)
    return globalCtx:getString(name)
end

function SetBox.getBool(name)
    return globalCtx:getBool(name)
end

-- =============================================================================
-- Script Loading
-- =============================================================================

function SetBox.loadFile(path)
    local chunk, err = loadfile(path, "t", _G)
    if not chunk then
        print("[SetBox] Error loading " .. path .. ": " .. tostring(err))
        return false
    end
    local ok, result = pcall(chunk)
    if not ok then
        print("[SetBox] Error executing " .. path .. ": " .. tostring(result))
        return false
    end
    return true
end

-- Factory for LWC
function SetBox.newContext(tags, parent)
    return Context.new(tags, parent)
end

return SetBox
