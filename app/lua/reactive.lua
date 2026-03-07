--[[
    reactive.lua - Reactive Property System

    Provides Observables and Computeds for automatic dependency tracking
    and lazy evaluation. Changes propagate through the dependency graph
    in topological order via lazy recomputation.

    Usage:
        local R = require("reactive")

        local width = R.observable(100)
        local height = R.observable(50)
        local area = R.computed(function()
            return width:get() * height:get()
        end)

        print(area:get())  -- 5000
        width:set(200)
        print(area:get())  -- 10000 (recomputed lazily)

        -- Watch for side effects
        R.watch(function()
            print("Area changed to: " .. area:get())
        end)
]]

local Reactive = {}

-- Current computed being evaluated (for dependency tracking)
local currentComputed = nil

-- Batch state
local batchDepth = 0
local pendingNotifications = {}

-- =============================================================================
-- Observable: A reactive value that tracks dependents
-- =============================================================================

function Reactive.observable(initialValue)
    local value = initialValue
    local dependents = {}  -- Set of computeds that depend on this

    local self = {}

    function self:get()
        -- Track dependency if we're inside a computed
        if currentComputed then
            dependents[currentComputed] = true
            currentComputed._dependencies[self] = true
        end
        return value
    end

    function self:set(newValue)
        if value == newValue then
            return false -- No change
        end
        value = newValue

        -- Mark all dependents as dirty
        for computed in pairs(dependents) do
            computed:_markDirty()
        end

        -- Notify watchers (batched if in batch mode)
        if batchDepth > 0 then
            pendingNotifications[self] = true
        end
        
        return true -- Value changed
    end

    function self:peek()
        -- Get value without tracking dependency
        return value
    end

    function self:_removeDependant(computed)
        dependents[computed] = nil
    end

    function self:_addDependant(computed)
        dependents[computed] = true
    end

    self._isObservable = true
    return self
end

-- =============================================================================
-- Computed: A derived value with automatic dependency tracking
-- =============================================================================

function Reactive.computed(computeFn)
    local cached = nil
    local dirty = true
    local dependents = {}  -- Computeds that depend on this computed

    local self = {}
    self._dependencies = {}  -- Observables/Computeds we read

    function self:get()
        -- Track dependency if we're inside another computed
        if currentComputed then
            dependents[currentComputed] = true
            currentComputed._dependencies[self] = true
        end

        if dirty then
            -- Clear old dependencies
            for dep in pairs(self._dependencies) do
                dep:_removeDependant(self)
            end
            self._dependencies = {}

            -- Track what we read during compute
            local prevComputed = currentComputed
            currentComputed = self

            local ok, result = pcall(computeFn)

            currentComputed = prevComputed

            if ok then
                cached = result
            else
                print("[Reactive] Computed error: " .. tostring(result))
                cached = nil
            end

            dirty = false
        end

        return cached
    end

    function self:peek()
        -- Get cached value without triggering recompute or tracking
        return cached
    end

    function self:_markDirty()
        if not dirty then
            dirty = true
            -- Propagate dirty to our dependents
            for dep in pairs(dependents) do
                dep:_markDirty()
            end
        end
    end

    function self:_removeDependant(computed)
        dependents[computed] = nil
    end

    function self:_addDependant(computed)
        dependents[computed] = true
    end

    function self:isDirty()
        return dirty
    end

    self._isComputed = true
    return self
end

-- =============================================================================
-- Watch: Run a function whenever its dependencies change
-- =============================================================================

local watchers = {}

function Reactive.watch(fn)
    local watcher = {}
    watcher._dependencies = {}
    local disposed = false

    local function run()
        if disposed then return end

        -- Clear old dependencies
        for dep in pairs(watcher._dependencies) do
            dep:_removeDependant(watcher)
        end
        watcher._dependencies = {}

        -- Track dependencies during execution
        local prevComputed = currentComputed
        currentComputed = watcher

        local ok, err = pcall(fn)

        currentComputed = prevComputed

        if not ok then
            print("[Reactive] Watch error: " .. tostring(err))
        end
    end

    function watcher:_markDirty()
        -- Watchers run immediately when marked dirty (unless batched)
        if batchDepth > 0 then
            pendingNotifications[watcher] = run
        else
            run()
        end
    end

    function watcher:_removeDependant()
        -- Watchers don't have dependents
    end

    function watcher:dispose()
        disposed = true
        for dep in pairs(watcher._dependencies) do
            dep:_removeDependant(watcher)
        end
        watcher._dependencies = {}
        watchers[watcher] = nil
    end

    watchers[watcher] = true

    -- Run immediately to establish initial dependencies
    run()

    return watcher
end

-- =============================================================================
-- Batch: Group multiple changes into one update cycle
-- =============================================================================

function Reactive.batch(fn)
    batchDepth = batchDepth + 1

    local ok, err = pcall(fn)

    batchDepth = batchDepth - 1

    if batchDepth == 0 then
        -- Flush pending notifications
        local pending = pendingNotifications
        pendingNotifications = {}
        for _, action in pairs(pending) do
            if type(action) == "function" then
                action()
            end
        end
    end

    if not ok then
        error(err)
    end
end

function Reactive.isBatching()
    return batchDepth > 0
end

-- =============================================================================
-- Utility: Create an observable object with multiple properties
-- =============================================================================

function Reactive.observableObject(initialValues)
    local obj = {}
    local observables = {}

    for key, value in pairs(initialValues or {}) do
        observables[key] = Reactive.observable(value)
    end

    setmetatable(obj, {
        __index = function(_, key)
            local obs = observables[key]
            if obs then
                return obs:get()
            end
            return nil
        end,
        __newindex = function(_, key, value)
            local obs = observables[key]
            if obs then
                obs:set(value)
            else
                -- Create new observable for new keys
                observables[key] = Reactive.observable(value)
            end
        end,
    })

    -- Expose raw observables for direct access
    obj._observables = observables

    return obj
end

-- =============================================================================
-- Utility: Check if value is reactive
-- =============================================================================

function Reactive.isObservable(value)
    return type(value) == "table" and value._isObservable == true
end

function Reactive.isComputed(value)
    return type(value) == "table" and value._isComputed == true
end

function Reactive.isReactive(value)
    return Reactive.isObservable(value) or Reactive.isComputed(value)
end

-- =============================================================================
-- Untrack: Read values without tracking dependencies
-- =============================================================================

function Reactive.untrack(fn)
    local prevComputed = currentComputed
    currentComputed = nil
    local ok, result = pcall(fn)
    currentComputed = prevComputed
    if ok then
        return result
    else
        error(result)
    end
end

return Reactive
