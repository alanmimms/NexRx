--[[
    Constraint Solver for Layout System

    Evaluates constraint expressions from SetBox rules to compute widget bounds.

    Two-pass approach:
    1. Bottom-up: Collect intrinsic sizes from leaves
    2. Top-down: Distribute available space via springs

    Constraint expressions are Lua code evaluated with context:
    - parent.width, parent.height: parent dimensions
    - content.width, content.height: sum/max of children (for containers)
    - window.width, window.height: root window dimensions
]]

local constraints = {}

-- Cache for compiled expressions
local exprCache = {}

-- Evaluate a constraint expression string with given context
-- expr: Lua expression string like "parent.width * 0.2" or "200"
-- ctx: table with parent, content, window dimensions
-- Returns: number
function constraints.eval(expr, ctx)
    if not expr then return nil end

    -- Numbers pass through directly
    if type(expr) == "number" then
        return expr
    end

    -- Try parsing as plain number first
    local num = tonumber(expr)
    if num then return num end

    -- Compile and cache the expression
    local fn = exprCache[expr]
    if not fn then
        local code = "return " .. expr
        -- Use ctx as the environment for the compiled function
        local compiled, err = load(code, "constraint", "t", ctx)
        if not compiled then
            print("[Constraints] Expression error: " .. tostring(err))
            return nil
        end
        fn = compiled
    end

    -- Evaluate with context
    local ok, result = pcall(fn)
    if not ok then
        print("[Constraints] Eval error: " .. tostring(result))
        return nil
    end

    return result
end

-- Query SetBox for constraint properties given widget tags
-- Returns table of constraint values (some may be expression strings)
function constraints.query(id, tags)
    local SetBox = require("SetBox")
    -- Save current tags and set query tags
    local prevTags = SetBox.getActiveTags()
    
    local queryTags = {}
    if tags then for _, t in ipairs(tags) do table.insert(queryTags, t) end end
    if id then table.insert(queryTags, "id." .. id) end
    
    SetBox.setActiveTags(queryTags)

    -- Properties we care about
    local props = {
        "anchorLeft", "anchorRight", "anchorTop", "anchorBottom",
        "width", "height",
        "minWidth", "maxWidth", "minHeight", "maxHeight",
        "marginInner", "marginOuter",
        "springX", "springY",
    }

    local result = {}
    for _, prop in ipairs(props) do
        if SetBox.has(prop) then
            local value = SetBox.get(prop)
            result[prop] = value
        end
    end

    -- Restore previous tags
    SetBox.setActiveTags(prevTags)
    return result
end

-- Compute bounds for a widget given constraints and parent context
-- cons: constraint properties from query()
-- parent: {x, y, width, height} of parent region
-- content: {width, height} of children (for containers), or nil
-- window: {width, height} of root window
-- Returns: {x, y, w, h} computed bounds
function constraints.solve(cons, parent, content, window)
    content = content or {width = 0, height = 0}

    -- Build evaluation context
    local ctx = {
        parent = {width = parent.width, height = parent.height},
        content = content,
        window = window,
        math = math,
        print = print,
    }

    -- Helper to evaluate constraint expression
    local function ev(expr)
        if not expr then return nil end
        if type(expr) == "number" then return expr end

        -- Rebuild function with current context
        local code = "return " .. expr
        local fn, err = load(code, "constraint", "t", ctx)
        if not fn then return nil end
        local ok, result = pcall(fn)
        return ok and result or nil
    end

    -- Evaluate size expressions
    local width = ev(cons.width)
    local height = ev(cons.height)
    local minW = ev(cons.minWidth)
    local maxW = ev(cons.maxWidth)
    local minH = ev(cons.minHeight)
    local maxH = ev(cons.maxHeight)

    -- Apply min/max bounds
    if width then
        if minW then width = math.max(minW, width) end
        if maxW then width = math.min(maxW, width) end
    end
    if height then
        if minH then height = math.max(minH, height) end
        if maxH then height = math.min(maxH, height) end
    end

    -- Default to parent size if not specified
    width = width or parent.width
    height = height or parent.height

    -- Compute position from anchors
    local x, y = parent.x, parent.y

    local anchorL = cons.anchorLeft or 0
    local anchorR = cons.anchorRight or 0
    local anchorT = cons.anchorTop or 0
    local anchorB = cons.anchorBottom or 0

    -- Horizontal positioning
    if anchorL > 0 and anchorR > 0 then
        -- Anchored both sides - stretch to fill (width becomes flexible)
        x = parent.x
        width = parent.width
    elseif anchorL > 0 then
        -- Anchored left
        x = parent.x
    elseif anchorR > 0 then
        -- Anchored right
        x = parent.x + parent.width - width
    else
        -- Centered (default) or use springs
        x = parent.x + (parent.width - width) / 2
    end

    -- Vertical positioning
    if anchorT > 0 and anchorB > 0 then
        -- Anchored both sides - stretch to fill
        y = parent.y
        height = parent.height
    elseif anchorT > 0 then
        -- Anchored top
        y = parent.y
    elseif anchorB > 0 then
        -- Anchored bottom
        y = parent.y + parent.height - height
    else
        -- Centered (default)
        y = parent.y + (parent.height - height) / 2
    end

    -- Apply margins
    local marginInner = cons.marginInner or 0
    local marginOuter = cons.marginOuter or 0

    -- Outer margin shrinks our bounds
    if marginOuter > 0 then
        local mx = width * marginOuter
        local my = height * marginOuter
        x = x + mx
        y = y + my
        width = width - mx * 2
        height = height - my * 2
    end

    -- Inner margin is for content (returned separately if needed)
    local innerX, innerY, innerW, innerH = x, y, width, height
    if marginInner > 0 then
        local px = width * marginInner
        local py = height * marginInner
        innerX = x + px
        innerY = y + py
        innerW = width - px * 2
        innerH = height - py * 2
    end

    return {
        x = x, y = y, w = width, h = height,
        innerX = innerX, innerY = innerY, innerW = innerW, innerH = innerH,
    }
end

-- Distribute available space among widgets with springs
-- widgets: list of {tags, constraints, minSize}
-- available: total available space
-- axis: "x" or "y"
-- Returns: list of computed sizes
function constraints.distributeSpring(widgets, available, axis)
    local springKey = axis == "x" and "springX" or "springY"
    local minKey = axis == "x" and "minWidth" or "minHeight"

    -- Sum spring strengths and minimum sizes
    local totalSpring = 0
    local totalMin = 0

    for _, w in ipairs(widgets) do
        local spring = w.constraints[springKey] or 0
        local minSize = w.minSize or 0
        totalSpring = totalSpring + spring
        totalMin = totalMin + minSize
    end

    -- Flexible space after minimums
    local flexible = math.max(0, available - totalMin)

    -- Distribute
    local sizes = {}
    for i, w in ipairs(widgets) do
        local spring = w.constraints[springKey] or 0
        local minSize = w.minSize or 0

        local size = minSize
        if totalSpring > 0 and spring > 0 then
            size = size + (flexible * spring / totalSpring)
        end

        sizes[i] = size
    end

    return sizes
end

return constraints
