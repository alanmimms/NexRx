--[[
    Hierarchical Container Layout Solver (Pure Spring-Constraint)

    Computes widget regions based on parent/child relationships defined in SetBox.
    Uses spring stiffness values (springTop, springBottom, springLeft, springRight) 
    to distribute surplus space proportional to 1/Stiffness (compliance).
]]

local container = {}
local SetBox = require("SetBox")
local Constraints = require("ui.Constraints")

-- Internal State (Reset every pass)
local parentToChildren = {}
local measurementCache = {}
local allRegions = {}

local function getWidgetProps(id, tags)
    -- This returns a table of resolved properties and their _LWC
    return Constraints.query(id, tags)
end

local function evalProp(prop, lwc, id, parentDim, windowDim)
    return Constraints.eval(prop, lwc, parentDim, windowDim, id)
end

local function measureRecursive(widget, windowDim, depth)
    if depth > 25 then return 0, 0 end
    
    local lwc = widget._props._LWC
    local children = parentToChildren[widget.id]
    
    local minW = evalProp(widget._props.width or widget._props.minWidth, lwc, widget.id, nil, windowDim) or 0
    local minH = evalProp(widget._props.height or widget._props.minHeight, lwc, widget.id, nil, windowDim) or 0
    
    if not children or #children == 0 then
        -- Default leaf size based on label if not specified
        if minW == 0 or minH == 0 then
            local label = evalProp(widget._props.label, lwc, widget.id, nil, windowDim) or widget.id
            local tw = measureText(label)
            local th = getLineHeight()
            if minW == 0 then minW = tw end
            if minH == 0 then minH = th end
        end
    else
        -- Compound size: sum of children + padding + spacing
        local padding = evalProp(widget._props.padding, lwc, widget.id, nil, windowDim) or 0
        local spacing = evalProp(widget._props.spacing, lwc, widget.id, nil, windowDim) or 0
        local isHoriz = (evalProp(widget._props.direction, lwc, widget.id, nil, windowDim) or "horizontal") == "horizontal"
        
        local totalW, totalH = 0, 0
        for i, child in ipairs(children) do
            local cw, ch = measureRecursive(child, windowDim, depth + 1)
            if isHoriz then
                totalW = totalW + cw + (i > 1 and spacing or 0)
                totalH = math.max(totalH, ch)
            else
                totalH = totalH + ch + (i > 1 and spacing or 0)
                totalW = math.max(totalW, cw)
            end
        end
        minW = math.max(minW, totalW + padding * 2)
        minH = math.max(minH, totalH + padding * 2)
    end

    measurementCache[widget.id] = { w = minW, h = minH }
    return minW, minH
end

local function solveRecursive(widget, x, y, w, h, windowDim, depth)
    if depth > 25 then return end
    
    allRegions[widget.id] = { x = x, y = y, w = w, h = h }
    
    local children = parentToChildren[widget.id]
    if not children or #children == 0 then return end
    
    local lwc = widget._props._LWC
    local padding = evalProp(widget._props.padding, lwc, widget.id, {width=w, height=h}, windowDim) or 0
    local spacing = evalProp(widget._props.spacing, lwc, widget.id, {width=w, height=h}, windowDim) or 0
    local isHoriz = (evalProp(widget._props.direction, lwc, widget.id, {width=w, height=h}, windowDim) or "horizontal") == "horizontal"
    
    local availW = w - padding * 2
    local availH = h - padding * 2
    local n = #children
    
    -- 1. Measure children and collect springs
    local totalMinSize = 0
    for i = 1, n do
        local child = children[i]
        local m = measurementCache[child.id]
        child._m = m
        local cLwc = child._props._LWC
        
        if isHoriz then
            totalMinSize = totalMinSize + m.w
            child._s1 = evalProp(child._props.springLeft, cLwc, child.id, {width=availW, height=availH}, windowDim) or 0
            child._s2 = evalProp(child._props.springRight, cLwc, child.id, {width=availW, height=availH}, windowDim) or 0
        else
            totalMinSize = totalMinSize + m.h
            child._s1 = evalProp(child._props.springTop, cLwc, child.id, {width=availW, height=availH}, windowDim) or 0
            child._s2 = evalProp(child._props.springBottom, cLwc, child.id, {width=availW, height=availH}, windowDim) or 0
        end
    end
    totalMinSize = totalMinSize + (n - 1) * spacing

    -- 2. Calculate Gap Stiffnesses (averaging opposing springs)
    local gapStiff = {}
    for i = 0, n do
        local s1 = (i == 0) and math.huge or children[i]._s2
        local s2 = (i == n) and math.huge or children[i+1]._s1
        gapStiff[i] = (s1 + s2) / 2
    end

    -- 3. Distribute Surplus (or Deficit)
    local surplus = (isHoriz and availW or availH) - totalMinSize
    local gapSizes = {}
    for i = 0, n do gapSizes[i] = (i > 0 and i < n) and spacing or 0 end

    if surplus ~= 0 then
        local compliances = {}
        local totalCompliance = 0
        for i = 0, n do
            local s = gapStiff[i]
            local c = 0
            if s < 0 then
                c = 1e12 -- Repulsive force (high compliance)
            elseif s > 0 and s ~= math.huge then
                c = 1 / s
            end
            compliances[i] = c
            totalCompliance = totalCompliance + c
        end

        if totalCompliance > 0 then
            for i = 0, n do
                gapSizes[i] = gapSizes[i] + surplus * (compliances[i] / totalCompliance)
            end
        elseif surplus < 0 then
            -- Pivot Rule: anchor at Top/Left, last gap absorbs deficit
            gapSizes[n] = gapSizes[n] + surplus
        end
    end

    -- 4. Final Positioning and Recursion
    local pos = isHoriz and (x + padding) or (y + padding)
    for i = 1, n do
        local child = children[i]
        pos = pos + gapSizes[i-1]
        
        local cw, ch
        if isHoriz then
            cw = child._m.w
            ch = availH
            solveRecursive(child, pos, y + padding, cw, ch, windowDim, depth + 1)
            pos = pos + cw
        else
            cw = availW
            ch = child._m.h
            solveRecursive(child, x + padding, pos, cw, ch, windowDim, depth + 1)
            pos = pos + ch
        end
    end
end

function container.solveAll(rootWidget, allWidgets, winW, winH)
    parentToChildren = {}; measurementCache = {}; allRegions = {}
    local windowDim = { width = winW, height = winH }
    
    -- Associate properties with widgets
    for _, w in ipairs(allWidgets) do
        w._props = getWidgetProps(w.id, w.tags)
    end

    -- Build hierarchy
    for _, w in ipairs(allWidgets) do
        local parentId = evalProp(w._props.parent, w._props._LWC, w.id, nil, windowDim)
        if parentId then
            if not parentToChildren[parentId] then parentToChildren[parentId] = {} end
            table.insert(parentToChildren[parentId], w)
        end
    end
    
    -- Sort by order
    for pId, children in pairs(parentToChildren) do
        table.sort(children, function(a, b)
            local oa = evalProp(a._props.order, a._props._LWC, a.id, nil, windowDim) or 0
            local ob = evalProp(b._props.order, b._props._LWC, b.id, nil, windowDim) or 0
            return oa < ob
        end)
    end

    measureRecursive(rootWidget, windowDim, 0)
    solveRecursive(rootWidget, 0, 0, winW, winH, windowDim, 0)
    
    return allRegions
end

-- Backward compatibility for old calls in Main.lua
function container.solve(winW, winH)
    -- This requires a global list of widgets.
    -- Main.lua should probably call solveAll instead.
    return nil
end

function container.getRegions()
    return allRegions
end

return container
