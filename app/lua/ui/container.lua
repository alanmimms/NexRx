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

local constraints = require("ui.constraints")
local layoutOverrides = require("layout_overrides")

-- Widget layout order (processed in sequence)
-- Widgets with same anchor+group share space proportionally via springs
local widgetOrder = {
    { id = "top-bar",       tags = {"widget.TopBar"},                             anchor = "top" },
    { id = "bottom-bar",    tags = {"widget.BottomBar"},                          anchor = "bottom" },
    { id = "left-sidebar",  tags = {"widget.Sidebar", "widget.LeftSidebar"},      anchor = "left" },
    -- Right group: sidebar and debug panel share right edge via springs
    { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"},     anchor = "right", group = "right" },
    { id = "active-tags",   tags = {"widget.DebugPanel"},                         anchor = "right", group = "right" },
    { id = "center",        tags = {"widget.CenterArea"},                         anchor = nil },  -- fills remaining
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

-- Solve layout for all widgets
-- Returns table of regions: { widgetId = {x, y, w, h}, ... }
function container.solve(winW, winH)
    local regions = {}
    local remaining = { x = 0, y = 0, w = winW, h = winH }
    local window = { width = winW, height = winH }
    local processedGroups = {}

    -- Collect widgets by group
    local groups = {}
    for _, widget in ipairs(widgetOrder) do
        if widget.group then
            groups[widget.group] = groups[widget.group] or {}
            table.insert(groups[widget.group], widget)
        end
    end

    for _, widget in ipairs(widgetOrder) do
        -- Skip if already processed as part of a group
        if widget.group and processedGroups[widget.group] then
            goto continue
        end

        local ctx = {
            parent = { width = remaining.w, height = remaining.h },
            window = window,
            math = math,
        }

        if widget.group then
            -- Process entire group together with springs
            local group = groups[widget.group]
            processedGroups[widget.group] = true

            -- For horizontal anchors (left/right), distribute width via springs
            -- For vertical anchors (top/bottom), distribute height via springs
            local isHorizontal = widget.anchor == "left" or widget.anchor == "right"

            -- Calculate total spring weight and min sizes
            local totalSpring = 0
            local totalMinSize = 0
            local widgetData = {}

            for _, w in ipairs(group) do
                local cons = constraints.query(w.tags)
                local spring = getSpring(w.id, isHorizontal and "x" or "y", cons, ctx)
                local size = getSize(w.id, isHorizontal and "width" or "height", cons, ctx)
                local minSize = isHorizontal
                    and (constraints.eval(cons.minWidth, ctx) or 100)
                    or (constraints.eval(cons.minHeight, ctx) or 50)

                totalSpring = totalSpring + spring
                totalMinSize = totalMinSize + minSize
                table.insert(widgetData, {
                    widget = w,
                    cons = cons,
                    spring = spring,
                    size = size or minSize,
                    minSize = minSize,
                })
            end

            -- Total space available for this group
            local availableSpace = isHorizontal and remaining.w or remaining.h
            local springSpace = math.max(0, availableSpace - totalMinSize)

            -- Distribute space and position widgets
            local cursor = 0
            for i, wd in ipairs(widgetData) do
                local w = wd.widget
                local springShare = (wd.spring / totalSpring) * springSpace
                local size = math.floor(wd.minSize + springShare)

                -- Apply max constraint
                local maxSize = isHorizontal
                    and constraints.eval(wd.cons.maxWidth, ctx)
                    or constraints.eval(wd.cons.maxHeight, ctx)
                if maxSize then size = math.min(size, maxSize) end

                -- Check for override (user dragged this widget)
                local overrideSize = layoutOverrides.get(w.id, isHorizontal and "width" or "height")
                if overrideSize then
                    size = overrideSize
                end

                local r = { x = remaining.x, y = remaining.y, w = remaining.w, h = remaining.h }

                if widget.anchor == "right" then
                    -- Right-anchored group: position from right edge, widgets stack left
                    r.x = remaining.x + remaining.w - cursor - size
                    r.w = size
                elseif widget.anchor == "left" then
                    -- Left-anchored group: position from left edge
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

            -- Reduce remaining space by total group size
            if widget.anchor == "right" or widget.anchor == "left" then
                if widget.anchor == "right" then
                    -- Don't change remaining.x for right anchor
                else
                    remaining.x = remaining.x + cursor
                end
                remaining.w = remaining.w - cursor
            else
                if widget.anchor == "bottom" then
                    -- Don't change remaining.y for bottom anchor
                else
                    remaining.y = remaining.y + cursor
                end
                remaining.h = remaining.h - cursor
            end
        else
            -- Single widget (no group) - original behavior
            local cons = constraints.query(widget.tags)
            local r = { x = remaining.x, y = remaining.y, w = remaining.w, h = remaining.h }

            if widget.anchor == "top" then
                local h = getSize(widget.id, "height", cons, ctx)
                h = clampHeight(h, cons, ctx) or 32
                r.h = h
                remaining.y = remaining.y + h
                remaining.h = remaining.h - h

            elseif widget.anchor == "bottom" then
                local h = getSize(widget.id, "height", cons, ctx)
                h = clampHeight(h, cons, ctx) or 28
                r.y = remaining.y + remaining.h - h
                r.h = h
                remaining.h = remaining.h - h

            elseif widget.anchor == "left" then
                local w = getSize(widget.id, "width", cons, ctx)
                w = clampWidth(w, cons, ctx) or 260
                r.w = w
                remaining.x = remaining.x + w
                remaining.w = remaining.w - w

            elseif widget.anchor == "right" then
                local w = getSize(widget.id, "width", cons, ctx)
                w = clampWidth(w, cons, ctx) or 200
                r.x = remaining.x + remaining.w - w
                r.w = w
                remaining.w = remaining.w - w

            else
                -- No anchor = fills remaining (center)
                -- r already equals remaining
            end

            regions[widget.id] = r
        end

        ::continue::
    end

    return regions
end

-- Get the widget order (for external iteration)
function container.getWidgetOrder()
    return widgetOrder
end

return container
