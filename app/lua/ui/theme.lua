--[[
  UI Theme - SetBox to Widget Style Bridge

  Queries SetBox for widget styles based on dynamic tags.
  Caches resolved styles per frame for performance.
]]

local theme = {}

-- Style cache (cleared each frame)
local styleCache = {}

-- Parse hex color to RGB (0-1 range)
function theme.hexToRgb(hex)
    if not hex or hex == "" then
        return 1, 1, 1
    end
    hex = hex:gsub("#", "")
    local r = tonumber(hex:sub(1, 2), 16) / 255
    local g = tonumber(hex:sub(3, 4), 16) / 255
    local b = tonumber(hex:sub(5, 6), 16) / 255
    return r or 1, g or 1, b or 1
end

-- Clear style cache (call at frame start)
function theme.beginFrame()
    styleCache = {}
end

-- Build cache key from tags
local function cacheKey(tags)
    return table.concat(tags, ",")
end

-- Get style for a widget with given tags
-- tags: array of strings like {"Button", "Primary", "Hovered"}
function theme.getStyle(tags)
    local key = cacheKey(tags)
    if styleCache[key] then
        return styleCache[key]
    end

    -- Set active tags in SetBox
    setbox.setActiveTags(tags)

    -- Query common style properties
    local style = {
        background = setbox.getString("background", "#2d2d3d"),
        foreground = setbox.getString("foreground", "#ffffff"),
        border = setbox.getString("border", "#404050"),
        accent = setbox.getString("accent", "#3b82f6"),
        borderRadius = setbox.getNumber("borderRadius", 4),
        borderWidth = setbox.getNumber("borderWidth", 1),
        padding = setbox.getNumber("padding", 8),
        fontSize = setbox.getNumber("fontSize", 14),
    }

    -- Parse colors to RGB
    style.bgR, style.bgG, style.bgB = theme.hexToRgb(style.background)
    style.fgR, style.fgG, style.fgB = theme.hexToRgb(style.foreground)
    style.borderR, style.borderG, style.borderB = theme.hexToRgb(style.border)
    style.accentR, style.accentG, style.accentB = theme.hexToRgb(style.accent)

    styleCache[key] = style
    return style
end

-- Get button style with state-based tags
function theme.getButtonStyle(baseTags, isHot, isActive, isDisabled)
    local tags = {"Button"}
    for _, t in ipairs(baseTags or {}) do
        table.insert(tags, t)
    end
    if isDisabled then
        table.insert(tags, "Disabled")
    elseif isActive then
        table.insert(tags, "Pressed")
    elseif isHot then
        table.insert(tags, "Hovered")
    end
    return theme.getStyle(tags)
end

-- Get panel style
function theme.getPanelStyle(baseTags)
    local tags = {"Panel"}
    for _, t in ipairs(baseTags or {}) do
        table.insert(tags, t)
    end
    return theme.getStyle(tags)
end

-- Get slider style
function theme.getSliderStyle(baseTags, isHot, isActive)
    local tags = {"Slider"}
    for _, t in ipairs(baseTags or {}) do
        table.insert(tags, t)
    end
    if isActive then
        table.insert(tags, "Active")
    elseif isHot then
        table.insert(tags, "Hovered")
    end
    return theme.getStyle(tags)
end

-- Get text input style
function theme.getInputStyle(baseTags, isHot, isFocused)
    local tags = {"Input"}
    for _, t in ipairs(baseTags or {}) do
        table.insert(tags, t)
    end
    if isFocused then
        table.insert(tags, "Focused")
    elseif isHot then
        table.insert(tags, "Hovered")
    end
    return theme.getStyle(tags)
end

-- Get checkbox style
function theme.getCheckboxStyle(baseTags, isHot, isActive, isChecked)
    local tags = {"Checkbox"}
    for _, t in ipairs(baseTags or {}) do
        table.insert(tags, t)
    end
    if isChecked then
        table.insert(tags, "Checked")
    end
    if isActive then
        table.insert(tags, "Pressed")
    elseif isHot then
        table.insert(tags, "Hovered")
    end
    return theme.getStyle(tags)
end

-- Get label style
function theme.getLabelStyle(baseTags)
    local tags = {"Label"}
    for _, t in ipairs(baseTags or {}) do
        table.insert(tags, t)
    end
    return theme.getStyle(tags)
end

return theme
