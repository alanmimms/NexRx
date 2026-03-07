--[[
  Audio Utilities Widget Module
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

local AudioUtils = {}
AudioUtils.__index = AudioUtils

function AudioUtils.new(state)
    local self = setmetatable({}, AudioUtils)
    self.state = state
    return self
end

function AudioUtils:draw(id, x, y, w, h)
    local style = theme.getStyle({"widget.Panel", "widget.AudioUtilsFrame"})
    
    drawRoundedRect(x, y, w, h, style.borderRadius or 6, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, 1)
    
    ui.label(id .. "-title", x + 12, y + 8, "AUDIO UTILS", {"Title"})
    
    local padding = 12
    local topMargin = 32
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    local cx, cy = layout.getCursor()
    ui.checkbox(id .. "-mute", "Master Mute", cx, cy, state.muteEnabled, {"MuteToggle"}, "muteEnabled")
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-filters", "Demod Filters", cx, cy, state.demodFilterEnabled, {"FilterToggle"}, "demodFilterEnabled")
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-tone", "440Hz Test Tone", cx, cy, state.testToneEnabled, {"TestToneToggle"}, "testToneEnabled")
    layout.newLine(28)
    
    layout.endRegion()
end

return AudioUtils
