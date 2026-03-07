--[[
  Audio Utilities Widget Module
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local AppState = require("app_state")

local AudioUtils = {}
AudioUtils.__index = AudioUtils

-- Define default rules for the Audio Utils widget
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
    local lwc = setbox.newContext({"widget.Panel", "widget.AudioUtilsFrame", "id." .. id})
    
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    
    ui.label(id .. "-title", x + 12, y + 8, lwc:getString("title"), {"Title"}, lwc)
    
    local padding = lwc:getNumber("padding")
    local topMargin = lwc:getNumber("topMargin")
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    local cx, cy = layout.getCursor()
    if ui.checkbox(id .. "-mute", lwc:getString("labelMute"), cx, cy, state.muteEnabled, {"MuteToggle"}, "muteEnabled", lwc) then
        AppState.set("muteEnabled", not state.muteEnabled)
    end
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    if ui.checkbox(id .. "-filters", lwc:getString("labelFilters"), cx, cy, state.demodFilterEnabled, {"FilterToggle"}, "demodFilterEnabled", lwc) then
        AppState.set("demodFilterEnabled", not state.demodFilterEnabled)
    end
    layout.newLine(28)
    
    cx, cy = layout.getCursor()
    if ui.checkbox(id .. "-tone", lwc:getString("labelTone"), cx, cy, state.testToneEnabled, {"TestToneToggle"}, "testToneEnabled", lwc) then
        AppState.set("testToneEnabled", not state.testToneEnabled)
    end
    layout.newLine(28)
    
    layout.endRegion()
end

return AudioUtils
