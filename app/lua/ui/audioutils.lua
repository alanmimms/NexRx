--[[
  Audio Utilities Widget Module
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

local AudioUtils = {}
AudioUtils.__index = AudioUtils

-- Default rules for the Audio Utils widget
setbox.rule {
    id = "audio-utils-defaults",
    tags = {"widget.AudioUtilsFrame"},
    priority = -50,
    apply = {
        title = "AUDIO UTILS",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        labelMute = "Master Mute",
        labelFilters = "Demod Filters",
        labelTone = "440Hz Test Tone",
    }
}

function AudioUtils.new(state)
    local self = setmetatable({}, AudioUtils)
    self.state = state
    return self
end

function AudioUtils:draw(id, x, y, w, h)
    local prevTags = setbox.getActiveTags()
    setbox.setActiveTags({"widget.Panel", "widget.AudioUtilsFrame", "id." .. id})
    
    local bgR, bgG, bgB = theme.hexToRgb(setbox.getString("background", "#242d42"))
    local bR, bG, bB = theme.hexToRgb(setbox.getString("border", "#3b82f6"))
    local radius = setbox.getNumber("borderRadius", 8)
    local alpha = setbox.getNumber("opacity", 1.0)
    local bWidth = setbox.getNumber("borderWidth", 1)
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    
    ui.label(id .. "-title", x + 12, y + 8, setbox.getString("title", "AUDIO UTILS"), {"Title"})
    
    local padding = setbox.getNumber("padding", 12)
    local topMargin = setbox.getNumber("topMargin", 32)
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    local cx, cy = layout.getCursor()
    ui.checkbox(id .. "-mute", setbox.getString("labelMute", "Master Mute"), cx, cy, state.muteEnabled, {"MuteToggle"}, "muteEnabled")
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-filters", setbox.getString("labelFilters", "Demod Filters"), cx, cy, state.demodFilterEnabled, {"FilterToggle"}, "demodFilterEnabled")
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-tone", setbox.getString("labelTone", "440Hz Test Tone"), cx, cy, state.testToneEnabled, {"TestToneToggle"}, "testToneEnabled")
    layout.newLine(28)
    
    layout.endRegion()
    setbox.setActiveTags(prevTags)
end

return AudioUtils
