--[[
    Container Layout Solver

    Computes widget regions based on anchors and springs.
    Recursive version that correctly calculates minimum sizes and distributes surplus space.
]]

local container = {}

local Constraints = require("ui.Constraints")
local SetBox = require("SetBox")

-- Force unbuffered output for debugging
io.stdout:setvbuf("no")

-- Internal State (Reset every pass)
local parentToChildren = {}
local resolvedWidgets = {}
local measurementCache = {}
local allRegions = {}

-- Virtual Root Definition
local virtualRoot = {
    { id = "top-bar",       tags = {"widget.TopBar"} },
    { id = "left-sidebar",  tags = {"widget.Sidebar", "widget.LeftSidebar"},      group = "main-row" },
    { id = "center-area",   tags = {"widget.CenterArea"},                         group = "main-row" },
    { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"},     group = "main-row" },
    { id = "active-tags",   tags = {"widget.Sidebar", "widget.ActiveTagsSidebar"}, group = "main-row" },
}

local function resolveWidgetProps(id, tags)
    local props = Constraints.query(id, tags)
    return {
        id = id,
        tags = tags,
        cons = props,
        group = props.group or nil,
        priority = props.order or 0
    }
end

local function getAnchor(w)
    if w.cons.stickLeft and not w.cons.stickRight then return "left"
    elseif w.cons.stickRight and not w.cons.stickLeft then return "right"
    elseif w.cons.stickBottom and not w.cons.stickTop then return "bottom"
    end
    return "top"
end

local measureRecursive
local solveRecursive

-- Helper to group children sequentially based on anchor and group ID
local function getSequentialGroups(children)
    local groups = {}
    if #children == 0 then return groups end
    
    local currentGroup = nil
    for _, child in ipairs(children) do
        local anchor = getAnchor(child)
        local gName = child.group or ("atomic-" .. child.id)
        
        if not currentGroup or currentGroup.name ~= gName or currentGroup.anchor ~= anchor then
            currentGroup = {
                name = gName,
                anchor = anchor,
                widgets = {},
                isH = (anchor == "left" or anchor == "right")
            }
            table.insert(groups, currentGroup)
        end
        table.insert(currentGroup.widgets, child)
    end
    return groups
end

measureRecursive = function(widgetId, parentW, parentH, windowSize, depth)
    if depth > 15 then return 50, 50 end
    
    local cacheKey = widgetId .. ":" .. parentW .. "x" .. parentH
    if measurementCache[cacheKey] then return measurementCache[cacheKey].w, measurementCache[cacheKey].h end

    local wData = resolvedWidgets[widgetId]
    if not wData then return 50, 50 end

    local children = parentToChildren[widgetId]
    local lwc = wData.cons._LWC
    local parentDim = { width = parentW, height = parentH }
    
    local ownMinW = Constraints.eval(wData.cons.minWidth or wData.cons.width, lwc, parentDim, windowSize) or 0
    local ownMinH = Constraints.eval(wData.cons.minHeight or wData.cons.height, lwc, parentDim, windowSize) or 0
    local spacing = Constraints.eval(wData.cons.spacing, lwc, parentDim, windowSize) or 0
    local padding = Constraints.eval(wData.cons.padding, lwc, parentDim, windowSize) or 0

    local resW, resH = 0, 0
    if not children or #children == 0 then
        resW, resH = math.max(ownMinW, 1), math.max(ownMinH, 1)
    else
        local groups = getSequentialGroups(children)
        local totalW, totalH = 0, 0
        local availW, availH = parentW - padding * 2, parentH - padding * 2

        for i, g in ipairs(groups) do
            local groupW, groupH = 0, 0
            for j, child in ipairs(g.widgets) do
                local cw, ch = measureRecursive(child.id, availW, availH, windowSize, depth + 1)
                if g.isH then
                    groupW = groupW + cw + (j < #g.widgets and spacing or 0)
                    groupH = math.max(groupH, ch)
                else
                    groupH = groupH + ch + (j < #g.widgets and spacing or 0)
                    groupW = math.max(groupW, cw)
                end
            end

            if g.isH then
                totalW = totalW + groupW + (i > 1 and spacing or 0)
                totalH = math.max(totalH, groupH)
            else
                totalH = totalH + groupH + (i > 1 and spacing or 0)
                totalW = math.max(totalW, groupW)
            end
        end
        resW, resH = math.max(totalW + padding * 2, ownMinW), math.max(totalH + padding * 2, ownMinH)
    end

    measurementCache[cacheKey] = { w = resW, h = resH }
    return resW, resH
end

solveRecursive = function(region, widgets, windowSize, parentWData)
    local lwc = parentWData and parentWData.cons._LWC or SetBox.newContext({})
    local parentDim = { width = region.w, height = region.h }
    local padding = Constraints.eval(parentWData and parentWData.cons.padding, lwc, parentDim, windowSize) or 0
    local spacing = Constraints.eval(parentWData and parentWData.cons.spacing, lwc, parentDim, windowSize) or 0

    local remaining = { 
        x = region.x + padding, 
        y = region.y + padding, 
        w = region.w - padding * 2, 
        h = region.h - padding * 2 
    }

    local groups = getSequentialGroups(widgets)
    if #groups == 0 then return end

    -- 1. Measure all groups and their total required extent
    local totalRequiredH = 0
    local totalRequiredW = 0
    local groupInfo = {}

    for i, g in ipairs(groups) do
        local gW, gH = 0, 0
        local items = {}
        for j, w in ipairs(g.widgets) do
            local iw, ih = measureRecursive(w.id, remaining.w, remaining.h, windowSize, 0)
            local springX = w.cons.springX or 0
            local springY = w.cons.springY or 0
            -- Default springs for stretched widgets
            if w.cons.stickLeft and w.cons.stickRight and springX == 0 then springX = 1 end
            if w.cons.stickTop and w.cons.stickBottom and springY == 0 then springY = 1 end
            
            table.insert(items, { w = w, minW = iw, minH = ih, springX = springX, springY = springY, curW = iw, curH = ih })
            
            if g.isH then
                gW = gW + iw + (j < #g.widgets and spacing or 0)
                gH = math.max(gH, ih)
            else
                gH = gH + ih + (j < #g.widgets and spacing or 0)
                gW = math.max(gW, iw)
            end
        end
        
        local info = { g = g, minW = gW, minH = gH, items = items, curW = gW, curH = gH }
        table.insert(groupInfo, info)
        
        -- Sum up the primary axis requirements
        -- NOTE: This logic assumes parent is either a vertical stack or horizontal row
        -- For sidebars, it's a vertical stack of groups.
        totalRequiredH = totalRequiredH + gH + (i > 1 and spacing or 0)
        totalRequiredW = math.max(totalRequiredW, gW)
    end

    -- 2. Distribute surplus space in the parent's primary axis (vertical for sidebars)
    local availH = remaining.h
    local surplusH = math.max(0, availH - totalRequiredH)
    
    if surplusH > 0 then
        -- Are there any vertical springs in any child group?
        local totalSpringY = 0
        for _, info in ipairs(groupInfo) do
            for _, item in ipairs(info.items) do totalSpringY = totalSpringY + item.springY end
        end
        
        if totalSpringY > 0 then
            -- Allocate to widget heights
            for _, info in ipairs(groupInfo) do
                local groupAddedH = 0
                for _, item in ipairs(info.items) do
                    if not info.g.isH then -- Vertical group: grow items
                        local added = surplusH * item.springY / totalSpringY
                        item.curH = item.curH + added
                        groupAddedH = groupAddedH + added
                    end
                end
                info.curH = info.curH + groupAddedH
            end
        else
            -- Magnetic distribution: Increase inter-group spacing if parent is stretched
            local parentIsStretched = parentWData and parentWData.cons.stickTop and parentWData.cons.stickBottom
            if parentIsStretched and #groupInfo > 1 then
                local extraSpacing = surplusH / (#groupInfo - 1)
                -- We'll apply this during positioning
                spacing = spacing + extraSpacing
            end
        end
    end

    -- 3. Position and solve recursively
    local currentY = remaining.y
    for _, info in ipairs(groupInfo) do
        local g = info.g
        local currentX = remaining.x
        
        -- For each group, if it's horizontal, it might have internal horizontal springs
        local groupSurplusW = math.max(0, remaining.w - info.minW)
        local groupTotalSpringX = 0
        for _, item in ipairs(info.items) do groupTotalSpringX = groupTotalSpringX + item.springX end
        
        local currentGroupSpacing = spacing
        if g.isH and groupTotalSpringX == 0 and groupSurplusW > 0 and #info.items > 1 then
            -- Horizontal magnetic spacing
            currentGroupSpacing = spacing + (groupSurplusW / (#info.items - 1))
        end

        for j, item in ipairs(info.items) do
            if g.isH and groupTotalSpringX > 0 then
                item.curW = item.curW + (groupSurplusW * item.springX / groupTotalSpringX)
            end
            
            -- Stretch in the secondary axis if requested
            local finalW = item.curW
            local finalH = item.curH
            if g.isH then -- Horizontal group
                if item.w.cons.stickTop and item.w.cons.stickBottom then finalH = remaining.h end
            else -- Vertical group
                if item.w.cons.stickLeft and item.w.cons.stickRight then finalW = remaining.w end
            end

            local r = { x = currentX, y = currentY, w = math.floor(finalW+0.5), h = math.floor(finalH+0.5) }
            
            -- Anchor adjustments within the group area
            if g.isH then
                if item.w.cons.stickBottom and not item.w.cons.stickTop then r.y = currentY + info.curH - finalH
                elseif not item.w.cons.stickTop and not item.w.cons.stickBottom then r.y = currentY + (info.curH - finalH)/2 end
            else
                if item.w.cons.stickRight and not item.w.cons.stickLeft then r.x = currentX + remaining.w - finalW
                elseif not item.w.cons.stickLeft and not item.w.cons.stickRight then r.x = currentX + (remaining.w - finalW)/2 end
            end

            allRegions[item.w.id] = r
            if parentToChildren[item.w.id] then solveRecursive(r, parentToChildren[item.w.id], windowSize, item.w) end
            
            if g.isH then
                currentX = currentX + item.curW + currentGroupSpacing
            else
                currentY = currentY + item.curH + spacing
            end
        end
        
        -- If it was a horizontal group, advance vertical cursor by group height
        if g.isH then
            currentY = currentY + info.curH + spacing
        end
    end
end

function container.solve(winW, winH)
    parentToChildren = {}; resolvedWidgets = {}; measurementCache = {}; allRegions = {}
    local windowSize = { width = winW, height = winH }
    
    -- Pre-index and resolve properties ONCE
    local allRules = SetBox.getRules()
    for _, rule in ipairs(allRules) do
        if rule.properties and rule.properties.parent then
            local pId = rule.properties.parent
            local tags = {}
            for t in pairs(rule.tags) do tags[t] = true end
            local tagList = {}
            for t in pairs(tags) do table.insert(tagList, t) end
            
            local resolved = resolveWidgetProps(rule.id, tagList)
            resolvedWidgets[rule.id] = resolved
            if not parentToChildren[pId] then parentToChildren[pId] = {} end
            table.insert(parentToChildren[pId], resolved)
        end
    end
    for _, list in pairs(parentToChildren) do table.sort(list, function(a,b) return a.priority < b.priority end) end

    -- Resolve virtual root
    local rootResolved = {}
    for _, v in ipairs(virtualRoot) do
        local res = resolveWidgetProps(v.id, v.tags)
        res.group = v.group -- Override virtual group
        resolvedWidgets[v.id] = res
        table.insert(rootResolved, res)
    end

    solveRecursive({x=0, y=0, w=winW, h=winH}, rootResolved, windowSize, nil)
    return allRegions
end

-- Compatibility function for older widget code
function container.solveDynamicSublayout(region, parentId)
    local subset = {}
    for id, r in pairs(allRegions) do
        local w = resolvedWidgets[id]
        if w and w.cons and w.cons.parent == parentId then
            subset[id] = r
        end
    end
    return subset
end

function container.getRegions()
    return allRegions
end

return container
