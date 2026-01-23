--[[
    layout_overrides.lua - Runtime layout overrides

    Stores user modifications to layout constraints.
    These override SetBox-resolved defaults.

    Key format: "widgetId:property" (e.g., "left-sidebar:width")
]]

local overrides = {}

local M = {}

-- Get an override value (or nil if not set)
function M.get(widgetId, property)
    local key = widgetId .. ":" .. property
    return overrides[key]
end

-- Set an override value
function M.set(widgetId, property, value)
    local key = widgetId .. ":" .. property
    overrides[key] = value
end

-- Get all overrides as a table
function M.getAll()
    return overrides
end

-- Load overrides from a table
function M.loadFrom(data)
    for k, v in pairs(data) do
        overrides[k] = v
    end
end

-- Clear all overrides
function M.clear()
    overrides = {}
end

-- Serialize to Lua code string
function M.serialize()
    local lines = {
        "-- config/layout-overrides.lua - User layout modifications",
        "-- Auto-generated, do not edit manually",
        "-- Modified: " .. os.date("%Y-%m-%d %H:%M:%S"),
        "",
        "return {",
    }

    for key, value in pairs(overrides) do
        if type(value) == "number" then
            table.insert(lines, string.format('    ["%s"] = %g,', key, value))
        elseif type(value) == "string" then
            table.insert(lines, string.format('    ["%s"] = %q,', key, value))
        end
    end

    table.insert(lines, "}")
    table.insert(lines, "")

    return table.concat(lines, "\n")
end

-- Save to file
function M.save(path)
    path = path or "config/layout-overrides.lua"
    local file = io.open(path, "w")
    if file then
        file:write(M.serialize())
        file:close()
        print("[LayoutOverrides] Saved: " .. path)
        return true
    end
    return false
end

-- Load from file
function M.load(path)
    path = path or "config/layout-overrides.lua"
    local file = io.open(path, "r")
    if file then
        local content = file:read("*a")
        file:close()
        local fn = load(content, path, "t", {})
        if fn then
            local ok, data = pcall(fn)
            if ok and type(data) == "table" then
                M.loadFrom(data)
                print("[LayoutOverrides] Loaded: " .. path)
                return true
            end
        end
    end
    return false
end

return M
