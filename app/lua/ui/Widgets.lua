--[[
  UI Widgets Utility Library
  Provides shared state and styling utilities for the modular widget system.
]]

local state = require("ui.State")
local layout = require("ui.Layout")

local ui = {}

-- Export new widget classes
ui.Label = require("ui.Label")
ui.Button = require("ui.Button")
ui.Checkbox = require("ui.Checkbox")
ui.Slider = require("ui.Slider")
ui.Panel = require("ui.Panel")
ui.SMeter = require("ui.SMeter")
ui.ActiveTags = require("ui.ActiveTags")
ui.GraticuleLegend = require("ui.GraticuleLegend")
ui.FrequencyDisplay = require("ui.FrequencyDisplay")

-- Forward state/event functions for convenience
ui.beginFrame = state.beginFrame
ui.endFrame = state.endFrame
ui.isHot = state.isHot
ui.isActive = state.isActive
ui.hasFocus = state.hasFocus
ui.wasClicked = state.wasClicked

function ui.setEventsModule(ev)
    state.setEventsModule(ev)
end

function ui.setLayoutModule(l)
    -- layout is already required locally
end

-- Parse hex color to RGB (0-1 range)
function ui.hexToRgb(hex)
    if not hex or hex == "" then
        return 1, 1, 1
    end
    hex = hex:gsub("#", "")
    if #hex < 6 then return 1, 1, 1 end
    local r = tonumber(hex:sub(1, 2), 16) / 255
    local g = tonumber(hex:sub(3, 4), 16) / 255
    local b = tonumber(hex:sub(5, 6), 16) / 255
    return r or 1, g or 1, b or 1
end

return ui
