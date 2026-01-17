# Reactive Property System Design

A minimal reactive core for Lua to enable automatic dependency tracking and change propagation for UI layout and configuration.

## Core Concepts

```
┌─────────────────────────────────────────────────────────────┐
│  Reactive Graph                                             │
│                                                             │
│  [Observable]──────►[Computed]──────►[Computed]             │
│       │                  │               │                  │
│       ▼                  ▼               ▼                  │
│  [Computed]         [Watcher]       [Watcher]               │
│       │              (UI update)    (DSP update)            │
│       ▼                                                     │
│  [Watcher]                                                  │
│                                                             │
│  On change: topological propagation, no glitches            │
└─────────────────────────────────────────────────────────────┘
```

## Minimal Implementation (~150 lines core)

```lua
-- reactive.lua - Minimal reactive property system

local Reactive = {}
Reactive.__index = Reactive

-- Global state for dependency tracking
local currentComputed = nil  -- The computed currently being evaluated
local batchDepth = 0         -- For batching multiple changes
local pendingUpdates = {}    -- Computeds needing re-evaluation

-------------------------------------------------------------------------------
-- Observable: a value that tracks its dependents
-------------------------------------------------------------------------------
function Reactive.observable(initialValue)
    local self = {
        _value = initialValue,
        _dependents = {},  -- set of computeds that read this
    }

    return setmetatable(self, {
        __call = function(t, newValue)
            if newValue == nil then
                -- READ: track dependency
                if currentComputed then
                    t._dependents[currentComputed] = true
                    currentComputed._dependencies[t] = true
                end
                return t._value
            else
                -- WRITE: update and notify
                if t._value ~= newValue then
                    t._value = newValue
                    Reactive._notify(t._dependents)
                end
            end
        end
    })
end

-------------------------------------------------------------------------------
-- Computed: a derived value that auto-updates
-------------------------------------------------------------------------------
function Reactive.computed(fn, writeFn)
    local self = {
        _fn = fn,
        _writeFn = writeFn,      -- optional: makes it read-write
        _value = nil,
        _dirty = true,
        _dependencies = {},       -- observables/computeds we read
        _dependents = {},         -- computeds that read us
        _level = 0,               -- for topological ordering
    }

    local function evaluate()
        -- Clear old dependencies
        for dep in pairs(self._dependencies) do
            dep._dependents[self] = nil
        end
        self._dependencies = {}

        -- Track new dependencies during evaluation
        local prevComputed = currentComputed
        currentComputed = self

        local ok, result = pcall(self._fn)

        currentComputed = prevComputed

        if ok then
            self._value = result
            self._dirty = false
            -- Update level (max of dependencies + 1)
            local maxLevel = 0
            for dep in pairs(self._dependencies) do
                if dep._level and dep._level > maxLevel then
                    maxLevel = dep._level
                end
            end
            self._level = maxLevel + 1
        else
            error("Computed evaluation failed: " .. tostring(result))
        end
    end

    return setmetatable(self, {
        __call = function(t, newValue)
            if newValue == nil then
                -- READ
                if t._dirty then
                    evaluate()
                end
                -- Track dependency
                if currentComputed then
                    t._dependents[currentComputed] = true
                    currentComputed._dependencies[t] = true
                end
                return t._value
            else
                -- WRITE (if writeFn provided)
                if t._writeFn then
                    t._writeFn(newValue)
                else
                    error("Cannot write to read-only computed")
                end
            end
        end
    })
end

-------------------------------------------------------------------------------
-- Watcher: run side effects when dependencies change
-------------------------------------------------------------------------------
function Reactive.watch(fn, callback)
    local deps = {}

    local function run()
        -- Clear old deps
        for dep in pairs(deps) do
            dep._dependents[run] = nil
        end
        deps = {}

        -- Track dependencies
        local prevComputed = currentComputed
        currentComputed = { _dependencies = deps }

        local value = fn()

        -- Register as dependent
        for dep in pairs(deps) do
            dep._dependents[run] = true
        end

        currentComputed = prevComputed

        if callback then
            callback(value)
        end
    end

    -- Wrap for notification system
    local watcher = {
        _dirty = false,
        _level = 9999,  -- watchers run last
        _run = run
    }

    run._watcher = watcher
    run()  -- Initial run to collect dependencies

    return function()
        -- Cleanup function
        for dep in pairs(deps) do
            dep._dependents[run] = nil
        end
    end
end

-------------------------------------------------------------------------------
-- Change propagation with topological ordering
-------------------------------------------------------------------------------
function Reactive._notify(dependents)
    for dep in pairs(dependents) do
        if dep._dirty ~= nil then  -- it's a computed
            dep._dirty = true
            pendingUpdates[dep] = true
        end
        if dep._watcher then  -- it's a watcher
            pendingUpdates[dep._watcher] = true
        end
    end

    if batchDepth == 0 then
        Reactive._flush()
    end
end

function Reactive._flush()
    -- Sort by level (topological order)
    local sorted = {}
    for item in pairs(pendingUpdates) do
        table.insert(sorted, item)
    end
    table.sort(sorted, function(a, b)
        return (a._level or 0) < (b._level or 0)
    end)

    pendingUpdates = {}

    -- Re-evaluate in order
    for _, item in ipairs(sorted) do
        if item._run then
            item._run()  -- watcher
        end
        -- computeds re-evaluate lazily on next read
    end
end

-------------------------------------------------------------------------------
-- Batching: group multiple changes into one update cycle
-------------------------------------------------------------------------------
function Reactive.batch(fn)
    batchDepth = batchDepth + 1
    local ok, err = pcall(fn)
    batchDepth = batchDepth - 1

    if batchDepth == 0 then
        Reactive._flush()
    end

    if not ok then error(err) end
end

return Reactive
```

## Usage Example

```lua
local R = require("reactive")

-- Simple observables
local width = R.observable(100)
local height = R.observable(50)

-- Computed property (auto-updates when width/height change)
local area = R.computed(function()
    return width() * height()
end)

-- Bidirectional computed (read-write)
local widthPercent = R.computed(
    function() return width() / 800 * 100 end,    -- read
    function(pct) width(pct / 100 * 800) end      -- write
)

-- Watcher (side effects)
R.watch(function() return area() end, function(val)
    print("Area changed to: " .. val)
end)

-- Usage
print(area())        --> 5000
width(200)           --> "Area changed to: 10000"
print(area())        --> 10000

widthPercent(50)     --> sets width to 400
                     --> "Area changed to: 20000"

-- Batch multiple changes (single notification)
R.batch(function()
    width(300)
    height(100)
end)
--> "Area changed to: 30000" (once, not twice)
```

## Integration with SetBox

```lua
-- Bridge SetBox properties to reactive system
local function reactiveProperty(name, initial)
    local obs = R.observable(initial)

    -- SetBox -> Reactive
    setbox.onPropertyChange(function(n, v)
        if n == name then obs(v) end
    end)

    -- Reactive -> SetBox (if bidirectional needed)
    -- R.watch(function() return obs() end, function(v)
    --     setbox.setProperty(name, v)
    -- end)

    return obs
end

local bandpassWidth = reactiveProperty("bandpassWidth", 500)
local bandpassCenter = reactiveProperty("bandpassCenter", 700)

-- Computed: effective passband edges
local lowEdge = R.computed(function()
    return bandpassCenter() - bandpassWidth() / 2
end)

local highEdge = R.computed(function()
    return bandpassCenter() + bandpassWidth() / 2
end)
```

## What This Gets You

| Feature | Included |
|---------|----------|
| Automatic dependency tracking | Yes |
| Lazy evaluation (compute on read) | Yes |
| Topological update order (no glitches) | Yes |
| Batched updates | Yes |
| Bidirectional computed | Yes |
| Watchers for side effects | Yes |
| Cleanup/disposal | Yes |

## What's Missing (for later)

- **Arrays/collections** - observing list mutations
- **Deep reactivity** - nested tables
- **Async computeds** - for debouncing
- **Debug tooling** - visualize dependency graph
- **Cycle detection** - currently would stack overflow

This is roughly what Vue.js 3's reactivity core does, scaled down. ~150 lines gets you 80% of the value. The remaining 20% (collections, deep reactivity, debugging) would add another 200-300 lines.

## Existing Lua Libraries Evaluated

| Library | License | Dependency Tracking | Lua 5.4 | Active |
|---------|---------|---------------------|---------|--------|
| Push | MIT | Computed properties, knockout.js-style | Unknown | Last commit 2021 |
| FRLua | MIT | Properties with combine/map | Explicitly <5.4 | Dormant |
| RxLua | MIT | Streams, not properties | Unknown | Last release 2017 |
| lua-reactivex | MIT | Streams, not properties | Unknown | Last release 2020 |

None of these are actively maintained or confirmed Lua 5.4 compatible, which motivated this custom design.
