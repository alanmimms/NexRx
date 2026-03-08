--[[
    Container Layout Solver

    Computes widget regions based on anchors and springs.
    Replaces the dock-based layout system.

    Processing order:
    1. Fixed-anchor widgets claim space from their edge (top, bottom, left, right)
    2. Widgets in the same group share space via springs (proportional weights)
    3. Center fills remaining space

    Each widget declares:
    - anchor: which edge to attach to (top/bottom/left/right/nil)
    - group: optional group name - widgets in same group share space via springs
    - springX/springY: weight for proportional distribution (default 1)
    - width/height: base size (used as minimum when springs active)
    - minWidth/maxWidth/minHeight/maxHeight: bounds
]]

local container = {}

local constraints = require("ui.Constraints")
local layoutOverrides = require("LayoutOverrides")
local setbox = require("SetBox")

-- Default rules for Container layout fallbacks (very low priority)
setbox.rule {
    id = "container-layout-defaults",
    tags = {"layout.System"}, -- Generic system tag
    priority = -2000,
    apply = {
        fallbackMinWidth = 50,
        fallbackMinHeight = 50,
        fallbackMaxWidth = 4000,
        fallbackMaxHeight = 4000,
        fallbackWidth = 260,
        fallbackHeight = 32,
    }
}

-- Widget layout order (processed in sequence)
-- Widgets with same anchor+group share space proportionally via springs
local widgetOrder = {
    { id = "top-bar",       tags = {"widget.TopBar"},                             anchor = "top" },
    -- Main horizontal row: all components share width via the spring solver
    { id = "left-sidebar",  tags = {"widget.Sidebar", "widget.LeftSidebar"},      anchor = "left", group = "main-row" },
    { id = "center-area",   tags = {"widget.CenterArea"},                         anchor = "left", group = "main-row" },
    { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"},     anchor = "left", group = "main-row" },
    { id = "active-tags",   tags = {"widget.DebugPanel"},                         anchor = "left", group = "main-row" },
}

-- Evaluate a constraint property, checking overrides first
local function getSize(widgetId, property, cons, ctx)
    -- Check overrides first
    local override = layoutOverrides.get(widgetId, property)
    if override then
        return override
    end

    -- Fall back to constraint expression
    local expr = cons[property]
    if not expr then return nil end

    return constraints.eval(expr, ctx)
end

-- Apply min/max constraints
local function clampWidth(size, cons, ctx)
    if not size then return nil end
    local minVal = constraints.eval(cons.minWidth, ctx)
    local maxVal = constraints.eval(cons.maxWidth, ctx)
    if minVal then size = math.max(minVal, size) end
    if maxVal then size = math.min(maxVal, size) end
    return size
end

local function clampHeight(size, cons, ctx)
    if not size then return nil end
    local minVal = constraints.eval(cons.minHeight, ctx)
    local maxVal = constraints.eval(cons.maxHeight, ctx)
    if minVal then size = math.max(minVal, size) end
    if maxVal then size = math.min(maxVal, size) end
    return size
end

-- Get spring weight for a widget (from constraints or default 1)
local function getSpring(widgetId, axis, cons, ctx)
    local prop = axis == "x" and "springX" or "springY"
    local override = layoutOverrides.get(widgetId, prop)
    if override then return override end

    local expr = cons[prop]
    if expr then
        return constraints.eval(expr, ctx)
    end
    return 1  -- Default spring weight
end

-- Solve layout for a set of widgets within a specific region
-- Used for nested layouts (e.g., spectrum+waterfall inside center area)
function container.solveSublayout(parentRegion, subWidgetOrder)
    local regions = {}
    local remaining = {
        x = parentRegion.x,
        y = parentRegion.y,
        w = parentRegion.w,
        h = parentRegion.h
    }
    -- Ensure window size is available for expressions that use it
    local winW, winH = getWindowSize()
    local window = { width = winW, height = winH }
    local processedGroups = {}

    -- Grouping logic
    local groups = {}
    for _, widget in ipairs(subWidgetOrder) do
        if widget.group then
            groups[widget.group] = groups[widget.group] or {}
            table.insert(groups[widget.group], widget)
        end
    end

    for _, widget in ipairs(subWidgetOrder) do
        if widget.group and processedGroups[widget.group] then
            goto continue
        end

        local ctx = {
            parent = { width = remaining.w, height = remaining.h },
            window = window,
            math = math,
        }

        if widget.group then
            -- Process entire group with robust iterative spring solver
            local group = groups[widget.group]
            processedGroups[widget.group] = true

            -- Determine orientation from group anchor
            local isHorizontal = widget.anchor == "left" or widget.anchor == "right"
            local availableSpace = isHorizontal and remaining.w or remaining.h
            
            local widgetData = {}
            local totalMinSize = 0

            -- Pass 1: Evaluate constraints and identify fixed/flexible widgets
            for _, w in ipairs(group) do
                local cons = constraints.query(w.tags)
                
                -- Evaluate strength (spring weight)
                local spring = getSpring(w.id, isHorizontal and "x" or "y", cons, ctx)
                
                -- Check for manual override
                local override = layoutOverrides.get(w.id, isHorizontal and "width" or "height")
                
                -- Generic system context for fallback lookups
                local systemLwc = setbox.newContext({"layout.System"})

                -- Determine min/max boundaries
                local minVal = isHorizontal
                    and constraints.eval(cons.minWidth, ctx)
                    or constraints.eval(cons.minHeight, ctx)
                
                -- If min is 0 or nil, fall back to base width/height property
                if not minVal or minVal < 1 then
                    minVal = isHorizontal
                        and (constraints.eval(cons.width, ctx))
                        or (constraints.eval(cons.height, ctx))
                end
                
                -- Finally, core system fallbacks if still zero/nil
                if not minVal or minVal < 1 then
                    minVal = isHorizontal
                        and systemLwc:getNumber("fallbackMinWidth")
                        or systemLwc:getNumber("fallbackMinHeight")
                end

                local maxVal = isHorizontal
                    and constraints.eval(cons.maxWidth, ctx)
                    or constraints.eval(cons.maxHeight, ctx)
                
                if not maxVal or maxVal < 1 then
                    maxVal = isHorizontal
                        and systemLwc:getNumber("fallbackMaxWidth")
                        or systemLwc:getNumber("fallbackMaxHeight")
                end
                
                -- If overridden, treat as fixed size with no strength
                if override then
                    minVal, maxVal, spring = override, override, 0
                end

                -- Clamp minimum to specified bounds
                if maxVal and minVal > maxVal then minVal = maxVal end

                totalMinSize = totalMinSize + minVal
                
                table.insert(widgetData, {
                    widget = w,
                    cons = cons,
                    spring = spring,
                    minSize = minVal,
                    maxSize = maxVal,
                    currentSize = minVal,
                    canGrow = (spring > 0) and (minVal < maxVal)
                })
            end

            -- Pass 2: Iteratively distribute remaining space using strength (proportional)
            local remainingToDistribute = math.max(0, availableSpace - totalMinSize)
            
            while remainingToDistribute > 0.5 do
                local totalStrength = 0
                for _, wd in ipairs(widgetData) do
                    if wd.canGrow then
                        totalStrength = totalStrength + wd.spring
                    end
                end

                if totalStrength <= 0 then break end

                local distributedInThisPass = 0
                for _, wd in ipairs(widgetData) do
                    if wd.canGrow then
                        local share = (wd.spring / totalStrength) * remainingToDistribute
                        local capacity = wd.maxSize - wd.currentSize
                        local growth = math.min(share, capacity)
                        
                        wd.currentSize = wd.currentSize + growth
                        distributedInThisPass = distributedInThisPass + growth
                        
                        if wd.currentSize >= wd.maxSize - 0.1 then
                            wd.canGrow = false
                        end
                    end
                end
                
                remainingToDistribute = remainingToDistribute - distributedInThisPass
                if distributedInThisPass < 0.1 then break end
            end

            -- Pass 3: Position widgets
            local cursor = 0
            for _, wd in ipairs(widgetData) do
                local w = wd.widget
                local size = math.floor(wd.currentSize + 0.5) -- Use rounded currentSize
                local r = { x = remaining.x, y = remaining.y, w = remaining.w, h = remaining.h }

                if widget.anchor == "right" then
                    r.x = remaining.x + remaining.w - cursor - size
                    r.w = size
                elseif widget.anchor == "left" then
                    r.x = remaining.x + cursor
                    r.w = size
                elseif widget.anchor == "top" then
                    r.y = remaining.y + cursor
                    r.h = size
                elseif widget.anchor == "bottom" then
                    r.y = remaining.y + remaining.h - cursor - size
                    r.h = size
                end

                regions[w.id] = r
                cursor = cursor + size
            end

            -- Adjust remaining space
            if isHorizontal then
                if widget.anchor == "left" then remaining.x = remaining.x + cursor end
                remaining.w = math.max(0, remaining.w - cursor)
            else
                if widget.anchor == "top" then remaining.y = remaining.y + cursor end
                remaining.h = math.max(0, remaining.h - cursor)
            end
        else
            -- Single widget logic
            local cons = constraints.query(widget.tags)
            local r = { x = remaining.x, y = remaining.y, w = remaining.w, h = remaining.h }

            local systemLwc = setbox.newContext({"layout.System"})
            if widget.anchor == "top" then
                local h = getSize(widget.id, "height", cons, ctx) or systemLwc:getNumber("fallbackHeight")
                r.h = h; remaining.y = remaining.y + h; remaining.h = remaining.h - h
            elseif widget.anchor == "bottom" then
                local h = getSize(widget.id, "height", cons, ctx) or systemLwc:getNumber("fallbackHeight")
                r.y = remaining.y + remaining.h - h; r.h = h; remaining.h = remaining.h - h
            elseif widget.anchor == "left" then
                local w = getSize(widget.id, "width", cons, ctx) or systemLwc:getNumber("fallbackWidth")
                r.w = w; remaining.x = remaining.x + w; remaining.w = remaining.w - w
            elseif widget.anchor == "right" then
                local w = getSize(widget.id, "width", cons, ctx) or systemLwc:getNumber("fallbackWidth")
                r.x = remaining.x + remaining.w - w; r.w = w; remaining.w = remaining.w - w
            end
            regions[widget.id] = r
        end

        ::continue::
    end

    return regions
end

-- Solve layout for all widgets
-- Returns table of regions: { widgetId = {x, y, w, h}, ... }
function container.solve(winW, winH)
    return container.solveSublayout({x = 0, y = 0, w = winW, h = winH}, widgetOrder)
end

-- Solve layout for widgets that declare a specific parent via SetBox rules
function container.solveDynamicSublayout(parentRegion, parentId)
    local dynamicOrder = {}
    local allRules = setbox.getRules()
    
    -- Find rules that have a 'parent' property matching parentId
    -- and identify the widget IDs
    local seenWidgets = {}
    for _, rule in ipairs(allRules) do
        if rule.properties and rule.properties.parent == parentId then
            -- We need a widget ID. If the rule is for a specific widget, 
            -- it should have it in its tags or we use its ID.
            local widgetId = nil
            for tag, _ in pairs(rule.tags or {}) do
                if tag:find("^id%.") then
                    widgetId = tag:sub(4)
                    break
                end
            end
            
            if widgetId and not seenWidgets[widgetId] then
                seenWidgets[widgetId] = true
                -- Query SetBox for this widget's full properties to get anchor, group, etc.
                local prevTags = setbox.getActiveTags()
                setbox.setActiveTags({"id." .. widgetId})
                
                table.insert(dynamicOrder, {
                    id = widgetId,
                    tags = {"id." .. widgetId},
                    anchor = setbox.getString("anchor"),
                    group = (setbox.has("group") and setbox.getString("group") ~= "") and setbox.getString("group") or "dynamic-sublayout",
                    priority = setbox.getNumber("order")
                })
                
                setbox.setActiveTags(prevTags)
            end
        end
    end
    
    -- Sort by order priority
    table.sort(dynamicOrder, function(a, b) return a.priority < b.priority end)
    
    local result = container.solveSublayout(parentRegion, dynamicOrder)
    return result
end

-- Get the widget order (for external iteration)
function container.getWidgetOrder()
    return widgetOrder
end

return container
