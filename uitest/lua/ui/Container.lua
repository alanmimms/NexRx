--[[
    Hierarchical Container Layout Solver (Pure Spring-Constraint)

    Computes widget regions based on parent/child relationships.
    Uses spring stiffness values (springTop, springBottom, springLeft, springRight) 
    to distribute surplus space.
    
    Physics:
    - Surplus is distributed proportional to 1/Stiffness (compliance).
    - math.huge = rigid (stuck).
    - S = 0 = Neutral (minimal expansion).
    - Negative surplus (overflow) anchors at Top/Left.
]]

local container = {}
local setbox = require("SetBox")


-- Internal State
local parentToChildren = {}
local measurementCache = {}
local allRegions = {}

local function evalProp(prop, lwc, id, parentDim, windowDim)
    if type(prop) ~= "function" then return prop end
    local evalCtx = { id = id, parent = parentDim, window = windowDim, lwc = lwc }
    setmetatable(evalCtx, { __index = function(_, k) return lwc[k] or _G[k] end })
    local ok, res = pcall(prop, evalCtx)
    return ok and res or nil
end

local function measureRecursive(widget, windowDim, depth)
    if depth > 20 then return 0, 0 end
    local props = Widget.getProps(widget.id, widget.tags)
    local lwc = props._lwc
    local children = parentToChildren[widget.id]
    
    local minW = evalProp(props.width or props.minWidth, lwc, widget.id, nil, windowDim) or 0
    local minH = evalProp(props.height or props.minHeight, lwc, widget.id, nil, windowDim) or 0
    
    if not children or #children == 0 then
        if minW == 0 or minH == 0 then
            local label = evalProp(props.label, lwc, widget.id, nil, windowDim) or widget.id
            local tw = measureText(label)
            local th = getLineHeight()
            if minW == 0 then minW = tw end
            if minH == 0 then minH = th end
        end
    else
        local padding = evalProp(props.padding, lwc, widget.id, nil, windowDim) or 0
        local spacing = evalProp(props.spacing, lwc, widget.id, nil, windowDim) or 0
        local isHoriz = (evalProp(props.direction, lwc, widget.id, nil, windowDim) or "horizontal") == "horizontal"
        
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
    if depth > 20 then return end
    allRegions[widget.id] = { x = x, y = y, w = w, h = h }
    
    local children = parentToChildren[widget.id]
    if not children or #children == 0 then return end
    
    local props = Widget.getProps(widget.id, widget.tags)
    local lwc = props._lwc
    local padding = evalProp(props.padding, lwc, widget.id, {w=w, h=h}, windowDim) or 0
    local spacing = evalProp(props.spacing, lwc, widget.id, {w=w, h=h}, windowDim) or 0
    local isHoriz = (evalProp(props.direction, lwc, widget.id, {w=w, h=h}, windowDim) or "horizontal") == "horizontal"
    
    local availW = w - padding * 2
    local availH = h - padding * 2
    local currentX = x + padding
    local currentY = y + padding

    -- 1. Identify Gaps and Stiffnesses
    -- Gaps: [0] Widget1 [1] Widget2 ... [N-1] WidgetN [N]
    local n = #children
    local stiff = {}
    local totalMinSize = 0
    
    for i = 1, n do
        local child = children[i]
        local m = measurementCache[child.id]
        local cProps = Widget.getProps(child.id, child.tags)
        local cLwc = cProps._lwc
        
        child._m = m
        child._props = cProps
        
        if isHoriz then
            totalMinSize = totalMinSize + m.w
            local sL = evalProp(cProps.springLeft, cLwc, child.id, {w=availW, h=availH}, windowDim) or 0
            local sR = evalProp(cProps.springRight, cLwc, child.id, {w=availW, h=availH}, windowDim) or 0
            child._sL = sL; child._sR = sR
        else
            totalMinSize = totalMinSize + m.h
            local sT = evalProp(cProps.springTop, cLwc, child.id, {w=availW, h=availH}, windowDim) or 0
            local sB = evalProp(cProps.springBottom, cLwc, child.id, {w=availW, h=availH}, windowDim) or 0
            child._sT = sT; child._sB = sB
        end
    end
    totalMinSize = totalMinSize + (n - 1) * spacing

    -- Calculate Gap Stiffnesses
    -- S_gap[i] = average of opposing springs
    local gapStiff = {}
    for i = 0, n do
        local s1, s2
        if isHoriz then
            s1 = (i == 0) and math.huge or children[i]._sR
            s2 = (i == n) and math.huge or children[i+1]._sL
        else
            s1 = (i == 0) and math.huge or children[i]._sB
            s2 = (i == n) and math.huge or children[i+1]._sT
        end
        gapStiff[i] = (s1 + s2) / 2
    end

    -- 2. Distribute Surplus
    local surplus = (isHoriz and availW or availH) - totalMinSize
    local gapSizes = {}
    for i = 0, n do gapSizes[i] = (i > 0 and i < n) and spacing or 0 end

    if surplus ~= 0 then
        local compliances = {}
        local totalCompliance = 0
        local hasRepulsive = false
        
        for i = 0, n do
            local s = gapStiff[i]
            local c = 0
            if s < 0 then
                hasRepulsive = true
                c = 1e12 -- Explosive repulsion
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
            -- Pivot Rule: If no springs are compliant and we have a deficit,
            -- the last gap absorbs it (overflow to the right/bottom).
            gapSizes[n] = gapSizes[n] + surplus
        end
    end

    -- 3. Final Positioning
    local pos = isHoriz and (x + padding) or (y + padding)
    for i = 1, n do
        local child = children[i]
        pos = pos + gapSizes[i-1]
        
        local cw, ch
        if isHoriz then
            cw = child._m.w
            ch = availH -- Stretch in secondary axis
            solveRecursive(child, pos, y + padding, cw, ch, windowDim, depth + 1)
            pos = pos + cw
        else
            cw = availW -- Stretch in secondary axis
            ch = child._m.h
            solveRecursive(child, x + padding, pos, cw, ch, windowDim, depth + 1)
            pos = pos + ch
        end
    end
end

function container.solveAll(rootWidget, allWidgets, winW, winH)
    parentToChildren = {}; measurementCache = {}; allRegions = {}
    local windowDim = { w = winW, h = winH }
    
    for _, w in ipairs(allWidgets) do
        local props = Widget.getProps(w.id, w.tags)
        local parentId = evalProp(props.parent, props._lwc, w.id, nil, windowDim)
        if parentId then
            if not parentToChildren[parentId] then parentToChildren[parentId] = {} end
            table.insert(parentToChildren[parentId], w)
        end
    end
    
    for pId, children in pairs(parentToChildren) do
        table.sort(children, function(a, b)
            local pa = Widget.getProps(a.id, a.tags)
            local pb = Widget.getProps(b.id, b.tags)
            return (pa.order or 0) < (pb.order or 0)
        end)
    end

    measureRecursive(rootWidget, windowDim, 0)
    solveRecursive(rootWidget, 0, 0, winW, winH, windowDim, 0)
    return allRegions
end

return container
