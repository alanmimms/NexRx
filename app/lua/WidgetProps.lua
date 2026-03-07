--[[
    widget_props.lua - Reactive Widget Property System

    Bridges SetBox rule resolution with reactive properties. Widgets have:
    - Observable properties that can be set directly
    - Computed properties derived from other widgets
    - Rule-resolved defaults from SetBox when no explicit value is set

    Usage:
        local WidgetProps = require("WidgetProps")

        -- Create a widget with reactive properties
        local sidebar = WidgetProps.create("sidebar", {
            x = 0,
            y = 0,
            width = 260,  -- Observable, can be changed
        })

        -- Create a widget whose position depends on sidebar
        local main = WidgetProps.create("main", {
            y = 0,
        })
        main:computed("x", function()
            return sidebar:get("width")
        end)

        -- When sidebar width changes, main.x updates automatically
        sidebar:set("width", 300)
        print(main:get("x"))  -- 300
]]

local R = require("Reactive")
local setbox = require("SetBox")

local WidgetProps = {}

-- Registry of all widgets
local widgets = {}

-- =============================================================================
-- Widget Property Container
-- =============================================================================

local Widget = {}
Widget.__index = Widget

function Widget.new(id, initialProps, tags)
    local self = setmetatable({}, Widget)

    self.id = id
    self.tags = tags or {}
    self._observables = {}  -- property name -> observable
    self._computeds = {}    -- property name -> computed
    self._watchers = {}     -- list of active watchers

    -- Initialize observable properties
    for name, value in pairs(initialProps or {}) do
        self._observables[name] = R.observable(value)
    end

    return self
end

--- Get a property value (checks computed, then observable, then rules)
function Widget:get(name)
    -- First check computeds
    local computed = self._computeds[name]
    if computed then
        return computed:get()
    end

    -- Then check observables
    local obs = self._observables[name]
    if obs then
        return obs:get()
    end

    -- Fall back to SetBox rule resolution
    return self:_resolveFromRules(name)
end

--- Set a property value (creates observable if needed)
function Widget:set(name, value)
    -- Can't set computed properties
    if self._computeds[name] then
        error("Cannot set computed property: " .. name)
    end

    local obs = self._observables[name]
    if obs then
        obs:set(value)
    else
        -- Create new observable
        self._observables[name] = R.observable(value)
    end
end

--- Define a computed property
function Widget:computed(name, fn)
    -- Remove existing computed if any
    if self._computeds[name] then
        self._computeds[name] = nil
    end

    self._computeds[name] = R.computed(fn)
    return self
end

--- Get the raw observable for a property (for direct reactive access)
function Widget:observable(name)
    return self._observables[name]
end

--- Watch a property for changes
function Widget:watch(name, fn)
    local watcher = R.watch(function()
        local value = self:get(name)
        fn(value, name, self)
    end)
    table.insert(self._watchers, watcher)
    return watcher
end

--- Watch any property change
function Widget:watchAll(fn)
    local watcher = R.watch(function()
        -- Read all observables to track them
        local props = {}
        for name, obs in pairs(self._observables) do
            props[name] = obs:get()
        end
        for name, comp in pairs(self._computeds) do
            props[name] = comp:get()
        end
        fn(props, self)
    end)
    table.insert(self._watchers, watcher)
    return watcher
end

--- Resolve property from SetBox rules using widget's tags
function Widget:_resolveFromRules(name)
    -- Temporarily set active tags to include widget tags
    local prevTags = setbox.getActiveTags()

    -- Build tag set: widget tags + current active tags
    local tagSet = {}
    for _, tag in ipairs(self.tags) do
        tagSet[tag] = true
    end
    for _, tag in ipairs(prevTags) do
        tagSet[tag] = true
    end

    local tagList = {}
    for tag in pairs(tagSet) do
        table.insert(tagList, tag)
    end

    setbox.setActiveTags(tagList)
    local value = setbox.get(name)
    setbox.setActiveTags(prevTags)

    return value
end

--- Add tags to this widget
function Widget:addTag(tag)
    for _, t in ipairs(self.tags) do
        if t == tag then return self end
    end
    table.insert(self.tags, tag)
    return self
end

--- Remove a tag from this widget
function Widget:removeTag(tag)
    for i, t in ipairs(self.tags) do
        if t == tag then
            table.remove(self.tags, i)
            return self
        end
    end
    return self
end

--- Check if widget has a tag
function Widget:hasTag(tag)
    for _, t in ipairs(self.tags) do
        if t == tag then return true end
    end
    return false
end

--- Dispose of all watchers
function Widget:dispose()
    for _, watcher in ipairs(self._watchers) do
        watcher:dispose()
    end
    self._watchers = {}
end

--- Get all property names
function Widget:getPropertyNames()
    local names = {}
    for name in pairs(self._observables) do
        names[name] = true
    end
    for name in pairs(self._computeds) do
        names[name] = true
    end
    local result = {}
    for name in pairs(names) do
        table.insert(result, name)
    end
    return result
end

--- Serialize to table (observables only, not computeds)
function Widget:toTable()
    local result = {
        id = self.id,
        tags = {},
        props = {},
    }
    for _, tag in ipairs(self.tags) do
        table.insert(result.tags, tag)
    end
    for name, obs in pairs(self._observables) do
        result.props[name] = obs:peek()
    end
    return result
end

-- =============================================================================
-- Module Functions
-- =============================================================================

--- Create a new widget with reactive properties
function WidgetProps.create(id, initialProps, tags)
    if widgets[id] then
        error("Widget already exists: " .. id)
    end
    local widget = Widget.new(id, initialProps, tags)
    widgets[id] = widget
    return widget
end

--- Get a widget by ID
function WidgetProps.get(id)
    return widgets[id]
end

--- Remove a widget
function WidgetProps.remove(id)
    local widget = widgets[id]
    if widget then
        widget:dispose()
        widgets[id] = nil
    end
end

--- Get all widgets
function WidgetProps.all()
    return widgets
end

--- Clear all widgets
function WidgetProps.clear()
    for _, widget in pairs(widgets) do
        widget:dispose()
    end
    widgets = {}
end

--- Batch multiple property changes
WidgetProps.batch = R.batch

--- Create a computed that depends on multiple widgets
function WidgetProps.computed(fn)
    return R.computed(fn)
end

--- Watch for changes across any widgets
function WidgetProps.watch(fn)
    return R.watch(fn)
end

-- Export the Widget class for direct use
WidgetProps.Widget = Widget

return WidgetProps
