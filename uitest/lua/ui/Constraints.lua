--[[
    Constraint Solver for Layout System

    Evaluates constraint expressions from SetBox rules to compute widget bounds.
]]

local constraints = {}
local SetBox = require("SetBox")

-- Cache for compiled expressions
local exprCache = {}

-- Evaluate a constraint expression with given LWC and layout context
function constraints.eval(expr, lwc, parentDim, windowDim, id)
    if expr == nil then return nil end
    local t = type(expr)
    if t == "number" or t == "boolean" then return expr end
    
    -- Create a hybrid context that supports both LWC methods and layout dimensions
    local evalCtx = setmetatable({
        parent = parentDim,
        window = windowDim,
        id = id
    }, { 
        __index = function(t, k)
            -- First check LWC methods/properties
            if lwc[k] then return lwc[k] end
            -- Fallback to global _G for math, string, etc.
            return _G[k]
        end
    })

    if t == "function" then
        local ok, result = pcall(expr, evalCtx)
        if not ok then
            print(string.format("[Constraints] Function eval error: %s", tostring(result)))
            return nil
        end
        return result
    end

    if t ~= "string" then return nil end
    local num = tonumber(expr)
    if num then return num end

    -- Avoid load() for simple property names or non-expressions
    if not (expr:find("parent") or expr:find("window") or expr:find("math") or expr:find(":")) then
        return nil
    end

    local factory = exprCache[expr]
    if not factory then
        local code = "return function(_ENV) return " .. expr .. " end"
        local compiled, err = load(code, "constraint", "t", nil)
        if not compiled then 
            print("[Constraints] Compile error in '" .. expr .. "': " .. tostring(err))
            return nil 
        end
        local ok, inner = pcall(compiled)
        if ok and type(inner) == "function" then
            factory = inner
            exprCache[expr] = factory
        else
            return nil
        end
    end

    local ok, result = pcall(factory, evalCtx)
    if not ok then 
        print(string.format("[Constraints] Eval error in '%s': %s", tostring(expr), tostring(result)))
        return nil 
    end
    return result
end

-- Query SetBox for constraint properties WITHOUT changing global state
function constraints.query(id, tags)
    local queryTags = {}
    if tags then for _, t in ipairs(tags) do table.insert(queryTags, t) end end
    if id then table.insert(queryTags, "id." .. id) end
    
    local ctx = SetBox.newContext(queryTags)
    local props = {
        "stickLeft", "stickRight", "stickTop", "stickBottom",
        "width", "height", "minWidth", "maxWidth", "minHeight", "maxHeight",
        "springX", "springY", "spacing", "padding", "order", "group"
    }

    local result = {}
    for _, prop in ipairs(props) do
        local ok, has = pcall(ctx.has, ctx, prop)
        if ok and has then
            local ok2, val = pcall(ctx.get, ctx, prop)
            if ok2 then result[prop] = val end
        end
    end

    -- Store the context for later evaluation of functions
    result._LWC = ctx
    return result
end

return constraints
