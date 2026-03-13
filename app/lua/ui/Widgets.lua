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
ui.SignalBox = require("ui.SignalBox")

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

return ui
