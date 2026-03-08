--[[
    Container Layout Solver

    Computes widget regions based on anchors and springs.
    Replaces the dock-based layout system.

    Processing order:
    1. Fixed-anchor widgets claim space from their edge (top, bottom, left, right).
    2. Proportional groups (with same anchor+group) share remaining space via springs.
    3. Evaluation context includes:
       - parent.width/height: available container space
       - window.width/height: screen dimensions
       - content.width/height: total intrinsic size of children (computed in Pass 1)
    
    Constraints are evaluated via SetBox rules:
    - width/height: specific size expressions
    - springX/springY: flexibility (higher = more expansion)
    - minWidth/maxWidth/minHeight/maxHeight: bounds
]]

local container = {}

local Constraints = require("ui.Constraints")
local LayoutOverrides = require("LayoutOverrides")
local SetBox = require("SetBox")

-- Default rules for Container layout fallbacks (very low priority)
SetBox.rule {
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
    { id = "left-sidebar",  tags = {"widget.Sidebar", "widget.LeftSidebar"},      anchor = "left",  group = "main-row" },
    { id = "center-area",   tags = {"widget.CenterArea"},                         anchor = "left",  group = "main-row" },
    { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"},     anchor = "left",  group = "main-row" },
    -- Dynamic widgets within right sidebar (handled separately in solveDynamicSublayout)
    { id = "active-tags",   tags = {"widget.ActiveTagsViewer"},                   anchor = "bottom" },
}

-- Helper to get strength from rules (defaults to 0 if not provided)
local function getSpring(id, axis, cons, ctx)
    local spring = (axis == "x") and cons.springX or cons.springY
    return tonumber(spring) or 0
end

-- Helper to get size from rules (evaluates expressions)
local function getSize(id, prop, cons, ctx)
    local expr = cons[prop]
    if not expr then return nil end
    return Constraints.eval(expr, ctx)
end

function container.solve(parentW, parentH)
    local parentRegion = { x = 0, y = 0, w = parentW, h = parentH }
    return container.solveSublayout(parentRegion, widgetOrder)
end

function container.solveDynamicSublayout(parentRegion, parentId)
    -- This function queries SetBox for all widgets claiming parentId as parent
    -- and builds a sub-widget order for them.
    local rules = SetBox.getRules()
    local dynamicOrder = {}
    local prevTags = SetBox.getActiveTags()
    
    for _, rule in ipairs(rules) do
        if rule.properties and rule.properties.parent == parentId then
            -- Found a widget belonging to this parent
            -- Extract ID from tags (format id.WIDGET_ID)
            local widgetId = nil
            for t in pairs(rule.tags) do
                widgetId = t:match("^id%.(.+)$")
                if widgetId then break end
            end
            
            if widgetId then
                -- Temporarily set context to this widget to resolve its layout rules
                SetBox.setActiveTags({"id." .. widgetId})
                
                table.insert(dynamicOrder, {
                    id = widgetId,
                    tags = {"id." .. widgetId},
                    anchor = SetBox.getString("anchor"),
                    group = (SetBox.has("group") and SetBox.getString("group") ~= "") and SetBox.getString("group") or "dynamic-sublayout",
                    priority = SetBox.getNumber("order")
                })
                
                SetBox.setActiveTags(prevTags)
            end
        end
    end
    
    -- Sort by priority/order
    table.sort(dynamicOrder, function(a, b) return a.priority < b.priority end)
    
    local result = container.solveSublayout(parentRegion, dynamicOrder)
    return result
end

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
    local windowSize = { width = winW, height = winH }
    
    local processedGroups = {}
    local groups = {}

    -- Pass 1: Identify groups and anchors
    for _, widget in ipairs(subWidgetOrder) do
        local gName = widget.group or ("single-" .. widget.id)
        if not groups[gName] then
            groups[gName] = { widgets = {}, anchor = widget.anchor or "top", isSingle = (widget.group == nil) }
            table.insert(processedGroups, gName)
        end
        table.insert(groups[gName].widgets, widget)
    end

    -- Pass 2: Process groups (including single widgets as groups of 1)
    for _, gName in ipairs(processedGroups) do
        local groupInfo = groups[gName]
        local group = groupInfo.widgets
        local anchor = groupInfo.anchor
        
        local isHorizontal = (anchor == "left" or anchor == "right")
        local availableSpace = isHorizontal and remaining.w or remaining.h
        
        local totalMinSize = 0
        local widgetData = {}
        local ctx = { parent = { width = remaining.w, height = remaining.h }, window = windowSize }

        -- Pass 2.1: Evaluate constraints for all widgets in group
        for _, w in ipairs(group) do
            local cons = Constraints.query(w.id, w.tags)
            local spring = getSpring(w.id, isHorizontal and "x" or "y", cons, ctx)
            local override = LayoutOverrides.get(w.id, isHorizontal and "width" or "height")
            local systemLwc = SetBox.newContext({"layout.System"})

            local minVal = isHorizontal
                and Constraints.eval(cons.minWidth, ctx)
                or Constraints.eval(cons.minHeight, ctx)
            
            if not minVal or minVal < 1 then
                minVal = isHorizontal
                    and (Constraints.eval(cons.width, ctx))
                    or (Constraints.eval(cons.height, ctx))
            end
            
            if not minVal or minVal < 1 then
                minVal = isHorizontal
                    and systemLwc:getNumber("fallbackMinWidth")
                    or systemLwc:getNumber("fallbackMinHeight")
            end

            local maxVal = isHorizontal
                and Constraints.eval(cons.maxWidth, ctx)
                or Constraints.eval(cons.maxHeight, ctx)
            
            if not maxVal or maxVal < 1 then
                maxVal = isHorizontal
                    and systemLwc:getNumber("fallbackMaxWidth")
                    or systemLwc:getNumber("fallbackMaxHeight")
            end
            
            if override then
                minVal, maxVal, spring = override, override, 0
            end
            if maxVal and minVal > maxVal then minVal = maxVal end

            totalMinSize = totalMinSize + minVal
            table.insert(widgetData, {
                widget = w, cons = cons, spring = spring,
                minSize = minVal, maxSize = maxVal, currentSize = minVal,
                canGrow = (spring > 0) and (minVal < maxVal)
            })
        end

        -- Pass 2.2: Distribute space (spring solver)
        local remainingToDistribute = math.max(0, availableSpace - totalMinSize)
        while remainingToDistribute > 0.5 do
            local totalStrength = 0
            for _, wd in ipairs(widgetData) do
                if wd.canGrow then totalStrength = totalStrength + wd.spring end
            end
            if totalStrength <= 0 then break end

            local distributedThisPass = 0
            for _, wd in ipairs(widgetData) do
                if wd.canGrow then
                    local share = (wd.spring / totalStrength) * remainingToDistribute
                    local oldSize = wd.currentSize
                    wd.currentSize = math.min(wd.maxSize, wd.currentSize + share)
                    local added = wd.currentSize - oldSize
                    distributedThisPass = distributedThisPass + added
                    if wd.currentSize >= wd.maxSize then wd.canGrow = false end
                end
            end
            remainingToDistribute = remainingToDistribute - distributedThisPass
            if distributedThisPass < 0.1 then break end
        end

        -- Pass 2.3: Position widgets and UPDATE REMAINING
        local cursor = 0
        local groupMaxOtherDim = 0
        local groupSumAnchorDim = 0
        
        for _, wd in ipairs(widgetData) do
            local w = wd.widget
            local size = math.floor(wd.currentSize + 0.5)
            local r = { x = remaining.x, y = remaining.y, w = remaining.w, h = remaining.h }

            if anchor == "right" then
                r.x = remaining.x + remaining.w - cursor - size; r.w = size
            elseif anchor == "left" then
                r.x = remaining.x + cursor; r.w = size
            elseif anchor == "top" then
                r.y = remaining.y + cursor; r.h = size
            elseif anchor == "bottom" then
                r.y = remaining.y + remaining.h - cursor - size; r.h = size
            end
            regions[w.id] = r
            cursor = cursor + size
            groupSumAnchorDim = groupSumAnchorDim + size
        end

        -- Update remaining space for next group
        if anchor == "top" then
            remaining.y = remaining.y + groupSumAnchorDim; remaining.h = remaining.h - groupSumAnchorDim
        elseif anchor == "bottom" then
            remaining.h = remaining.h - groupSumAnchorDim
        elseif anchor == "left" then
            remaining.x = remaining.x + groupSumAnchorDim; remaining.w = remaining.w - groupSumAnchorDim
        elseif anchor == "right" then
            remaining.w = remaining.w - groupSumAnchorDim
        end
    end
return regions
end

-- Get the widget order (for external iteration)
function container.getWidgetOrder()
return widgetOrder
end

return container
