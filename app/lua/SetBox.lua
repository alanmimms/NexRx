--[[
  SetBox - Rule-based Configuration and Styling System
  
  SetBox manages a collection of rules that apply properties based on 
  active "tags" and context. It is used for everything from hardware 
  defaults to UI styling and localization.
]]

local R = require("Reactive")
local SetBox = {}
_G.setbox = SetBox
_G.rule = function(def) return SetBox.rule(def) end

-- Internal state
local rules = {}
local rulesByKey = {} -- Map of property name -> table of rules providing it
-- activeTags is now a map of name -> observable(boolean)
local activeTags = {} 
local globalContext = {} -- Global context values
local nextRuleOrder = 1
local changeCallbacks = {}

-- Optimization: Track which tags are actually in use by rules to skip irrelevant updates
local knownTags = {}

-- Optimization: Reactive versioning for rules
local globalRulesVersion = R.observable(0)
local propertyVersions = {} -- Map of property name -> observable(version)

-- Cache for resolved properties (Global context only)
local cachedProperties = {}
local cacheValid = false

-- =============================================================================
-- Internal Helpers
-- =============================================================================

local function getPropertyVersion(name)
    if not propertyVersions[name] then
        propertyVersions[name] = R.observable(0)
    end
    return propertyVersions[name]
end

--- Get or create an observable for a tag
local function getTagObservable(name)
    if not activeTags[name] then
        activeTags[name] = R.observable(false)
    end
    return activeTags[name]
end

--- Parse a tag string like "tag@10" into name and priority
local function parseTag(tagStr)
    if not tagStr then return "nil", 0 end
    local name, pri = tagStr:match("([^@]+)@?(%d*)")
    return name or tagStr, tonumber(pri) or 0
end

-- =============================================================================
-- Context Object (LWC - Local Widget Context)
-- =============================================================================

local Context = {}
Context.__index = Context

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
    
    -- Reactive tracking: Reading the tag's state registers it as a dependency
    -- We only care about tags that actually exist in rules
    if knownTags[tag] then
        return getTagObservable(tag):get() == true
    end
    
    -- For non-rule tags, just peek to avoid creating useless observables
    local obs = activeTags[tag]
    return obs and obs:peek() == true
end

--- Get all tags contributing to this context (for error reporting)
function Context:getHierarchyTags()
    local tags = {}
    for name, obs in pairs(activeTags) do 
        if obs:peek() then tags[name] = true end 
    end
    if self.parent then
        local pTags = self.parent:getHierarchyTags()
        for _, t in ipairs(pTags) do tags[t] = true end
    end
    for t in pairs(self.localTags) do tags[t] = true end
    
    local list = {}
    for t in pairs(tags) do table.insert(list, t) end
    return list
end

function Context:getMatchingRules(propertyName, ruleToExclude)
    -- REACTIVE DEPENDENCY: 
    -- If we're looking for a specific property, depend only on THAT property's rules.
    -- Otherwise, depend on the global rule set version.
    if propertyName then
        getPropertyVersion(propertyName):get()
    else
        globalRulesVersion:get()
    end

    local matching = {}
    local source = rules
    if propertyName then
        source = rulesByKey[propertyName]
        if not source then return matching end
    end

    for _, rule in ipairs(source) do
        if rule.enabled and rule ~= ruleToExclude then
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

function Context:get(name, ruleToExclude)
    local matching = self:getMatchingRules(name, ruleToExclude)
    if #matching == 0 then
        local tagList = table.concat(self:getHierarchyTags(), ", ")
        error(string.format("[SetBox] Property '%s' not found in any matching rule.\nContext Tags: [%s]", name, tagList), 2)
    end
    
    -- Find the first rule in the sorted list that actually contains this property
    for _, m in ipairs(matching) do
        local val = m.rule.properties[name]
        if val ~= nil then
            if type(val) == "function" then
                return val(self, m.rule)
            end
            return val
        end
    end
    
    local tagList = table.concat(self:getHierarchyTags(), ", ")
    local ruleIds = {}
    for _, m in ipairs(matching) do table.insert(ruleIds, m.rule.id) end
    error(string.format("[SetBox] Property '%s' found in matching rules [%s] but with nil value.\nContext Tags: [%s]", 
        name, table.concat(ruleIds, ", "), tagList), 2)
end

function Context:has(name, ruleToExclude)
    local matching = self:getMatchingRules(name, ruleToExclude)
    if #matching == 0 then return false end
    
    for _, m in ipairs(matching) do
        if m.rule.properties[name] ~= nil then
            return true
        end
    end
    return false
end

function Context:getNumber(name, ruleToExclude)
    local val = self:get(name, ruleToExclude)
    if type(val) == "number" then return val end
    local num = tonumber(val)
    if not num then
        error(string.format("[SetBox] Property '%s' expected number, got %s", name, type(val)), 2)
    end
    return num
end

function Context:getString(name, ruleToExclude)
    local val = self:get(name, ruleToExclude)
    return tostring(val)
end

function Context:getBool(name, ruleToExclude)
    local val = self:get(name, ruleToExclude)
    if type(val) == "boolean" then return val end
    return val == true or val == "true" or val == 1 or val == "1" or val == "yes"
end

function Context:optNumber(name, default)
    local matching = self:getMatchingRules(name)
    if #matching == 0 then return default end
    for _, m in ipairs(matching) do
        local val = m.rule.properties[name]
        if val ~= nil then
            if type(val) == "function" then val = val(self, m.rule) end
            return tonumber(val) or default
        end
    end
    return default
end

function Context:optString(name, default)
    local matching = self:getMatchingRules(name)
    if #matching == 0 then return default end
    for _, m in ipairs(matching) do
        local val = m.rule.properties[name]
        if val ~= nil then
            if type(val) == "function" then val = val(self, m.rule) end
            return tostring(val)
        end
    end
    return default
end

function Context:optBool(name, default)
    local matching = self:getMatchingRules(name)
    if #matching == 0 then return default end
    for _, m in ipairs(matching) do
        local val = m.rule.properties[name]
        if val ~= nil then
            if type(val) == "function" then val = val(self, m.rule) end
            if type(val) == "boolean" then return val end
            return val == true or val == "true" or val == 1 or val == "1" or val == "yes"
        end
    end
    return default
end

local cacheValid = false
local globalCtx = nil -- Forward declaration

local function _invalidateCache(propertyName)
    cacheValid = false
    
    -- Increment version counter
    if propertyName then
        local obs = getPropertyVersion(propertyName)
        obs:set(obs:peek() + 1)
    else
        globalRulesVersion:set(globalRulesVersion:peek() + 1)
    end

    if #changeCallbacks == 0 or not globalCtx then return end
    
    cachedProperties = globalCtx:resolve()
    cacheValid = true

    -- Notify callbacks
    for _, cb in ipairs(changeCallbacks) do
        for name, value in pairs(cachedProperties) do
            pcall(cb, name, value)
        end
    end
end

function Context:addTag(tag)
    if not self.localTags then self.localTags = {} end
    table.insert(self.localTags, tag)
    _invalidateCache()
end

globalCtx = setmetatable({localTags = {}}, Context)

-- =============================================================================
-- Public API - Tag Management
-- =============================================================================

function SetBox.addTag(tag)
    local name, _ = parseTag(tag)
    local obs = getTagObservable(name)
    if not obs:peek() then
        obs:set(true)
        _invalidateCache()
    end
end

function SetBox.removeTag(tag)
    local name, _ = parseTag(tag)
    local obs = getTagObservable(name)
    if obs:peek() then
        obs:set(false)
        _invalidateCache()
    end
end

-- Optimization: Fast tag set comparison
local lastTagsSet = {}

function SetBox.setActiveTags(tags)
    local changed = false
    local newTagsSet = {}
    local newCount = 0
    
    for _, tag in ipairs(tags) do
        local name, _ = parseTag(tag)
        newTagsSet[name] = true
        newCount = newCount + 1
        if not lastTagsSet[name] then changed = true end
    end
    
    if not changed then
        -- Also check if any tags were removed
        local oldCount = 0
        for _ in pairs(lastTagsSet) do oldCount = oldCount + 1 end
        if newCount ~= oldCount then changed = true end
    end
    
    if not changed then return end
    lastTagsSet = newTagsSet

    R.batch(function()
        -- Update existing and add new
        for name, _ in pairs(newTagsSet) do
            getTagObservable(name):set(true)
        end
        
        -- Clear old tags not in new set
        for name, obs in pairs(activeTags) do
            if not newTagsSet[name] then
                obs:set(false)
            end
        end
        _invalidateCache()
    end)
end

function SetBox.getActiveTags()
    local result = {}
    for name, obs in pairs(activeTags) do 
        if obs:peek() then table.insert(result, name) end 
    end
    return result
end

function SetBox.hasTag(tag)
    local obs = activeTags[tag]
    return obs and obs:peek() == true
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

function SetBox._clear()
    rules = {}
    rulesByKey = {}
    activeTags = {}
    lastTagsSet = {}
    globalContext = {}
    nextRuleOrder = 1
    changeCallbacks = {}
    cachedProperties = {}
    cacheValid = false
    globalRulesVersion:set(0)
    propertyVersions = {}
    knownTags = {}
end

-- =============================================================================
-- Public API - Rule Registration
-- =============================================================================

function SetBox.rule(def)
    local id = def.id or ("rule_" .. tostring(nextRuleOrder))
    
    -- Check for existing rule with this ID to replace it
    local existingIndex = nil
    for i, r in ipairs(rules) do
        if r.id == id then
            existingIndex = i
            break
        end
    end

    local rule = {
        id = id,
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
            knownTags[name] = true
        end
    end
    if def.tag then
        local name, pri = parseTag(def.tag)
        rule.tags[name] = pri
        knownTags[name] = true
    end

    -- Process properties
    local function addProp(name, value)
        if value == nil then return end
        rule.properties[name] = value
    end

    if def.apply then
        for name, value in pairs(def.apply) do addProp(name, value) end
    end

    -- Direct properties shorthand
    local reserved = {id=1, tags=1, tag=1, priority=1, enabled=1, apply=1, ["when"]=1}
    for k, v in pairs(def) do
        if not reserved[k] then addProp(k, v) end
    end

    -- If replacement, clean up old references
    if existingIndex then
        local oldRule = rules[existingIndex]
        for name, _ in pairs(oldRule.properties) do
            local keyTable = rulesByKey[name]
            if keyTable then
                for i = #keyTable, 1, -1 do
                    if keyTable[i] == oldRule then table.remove(keyTable, i) end
                end
            end
        end
        table.remove(rules, existingIndex)
    end

    -- Add to global list
    table.insert(rules, rule)

    -- Add to property-specific lists (ensures correct order for precedence)
    for name, _ in pairs(rule.properties) do
        if not rulesByKey[name] then rulesByKey[name] = {} end
        table.insert(rulesByKey[name], rule)
        _invalidateCache(name)
    end
    
    return rule
end

function SetBox.getRules() return rules end

-- =============================================================================
-- Public API - Resolution (Global / Legacy)
-- =============================================================================

function SetBox.resolve()
    if cacheValid then return cachedProperties end
    cachedProperties = globalCtx:resolve()
    cacheValid = true
    return cachedProperties
end

function SetBox.getMatchingRules() return globalCtx:getMatchingRules() end
function SetBox.has(name) return globalCtx:has(name) end
function SetBox.get(name) return globalCtx:get(name) end
function SetBox.getNumber(name) return globalCtx:getNumber(name) end
function SetBox.getString(name) return globalCtx:getString(name) end
function SetBox.getBool(name) return globalCtx:getBool(name) end
function SetBox.optNumber(name, default) return globalCtx:optNumber(name, default) end
function SetBox.optString(name, default) return globalCtx:optString(name, default) end

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

function SetBox.newContext(tags, parent) return Context.new(tags, parent) end

return SetBox
