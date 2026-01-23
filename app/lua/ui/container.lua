--[[
    Container Layout Solver

    Computes widget regions based on anchors and springs.
    Replaces the dock-based layout system.

    Processing order:
    1. Fixed-anchor widgets claim space from their edge (top, bottom, left, right)
    2. Spring widgets divide remaining space proportionally

    Each widget declares:
    - anchorTop/Bottom/Left/Right: which edge to attach to
    - width/height: explicit size (or springX/springY for flexible)
    - minWidth/maxWidth/minHeight/maxHeight: bounds
]]

local container = {}

local constraints = require("ui.constraints")
local layoutOverrides = require("layout_overrides")

-- Widget layout order (processed in sequence)
-- Format: { id = widgetId, tags = {tags}, anchor = "top"|"bottom"|"left"|"right"|nil }
local widgetOrder = {
    { id = "top-bar",       tags = {"widget.TopBar"},                             anchor = "top" },
    { id = "bottom-bar",    tags = {"widget.BottomBar"},                          anchor = "bottom" },
    { id = "left-sidebar",  tags = {"widget.Sidebar", "widget.LeftSidebar"},      anchor = "left" },
    { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"},     anchor = "right" },
    { id = "active-tags",   tags = {"widget.DebugPanel"},                         anchor = "right" },
    { id = "center",        tags = {"widget.CenterArea"},                         anchor = nil },  -- fills remaining
}

-- Evaluate a constraint property, checking overrides first
local function getSize(widgetId, property, cons, ctx)
    -- Check overrides first
    local override = layoutOverrides.get(widgetId, property)

    -- Debug: always log override check for right-sidebar
    if container.debug and widgetId == "right-sidebar" and property == "width" then
        local f = io.open("/tmp/container-debug.log", "a")
        if f then
            f:write(string.format("getSize: override=%s\n", tostring(override)))
            f:close()
        end
    end

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

-- Debug flag
container.debug = false

-- Solve layout for all widgets
-- Returns table of regions: { widgetId = {x, y, w, h}, ... }
function container.solve(winW, winH)
    local regions = {}
    local remaining = { x = 0, y = 0, w = winW, h = winH }
    local window = { width = winW, height = winH }

    -- Debug: log winW to see if it changes during drag
    if container.debug then
        local f = io.open("/tmp/container-debug.log", "a")
        if f then
            f:write(string.format("container.solve: winW=%d\n", winW))
            f:close()
        end
    end

    for _, widget in ipairs(widgetOrder) do
        local cons = constraints.query(widget.tags)
        local ctx = {
            parent = { width = remaining.w, height = remaining.h },
            window = window,
            math = math,
        }

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
            -- Debug: log remaining.w and computed x for right-sidebar
            if container.debug and widget.id == "right-sidebar" then
                local f = io.open("/tmp/container-debug.log", "a")
                if f then
                    f:write(string.format("  right-sidebar: remaining.w=%d w=%d -> x=%d\n", remaining.w + w, w, r.x))
                    f:close()
                end
            end
            remaining.w = remaining.w - w

        else
            -- No anchor = fills remaining (center)
            -- r already equals remaining
        end

        regions[widget.id] = r
    end

    return regions
end

-- Get the widget order (for external iteration)
function container.getWidgetOrder()
    return widgetOrder
end

return container
