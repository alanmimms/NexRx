--[[
  AGC Widget Module
  Modular UI component for Automatic Gain Control.
  Style and layout driven entirely by SetBox rules.
]]

local ui = require("ui.Widgets")
local layout = require("ui.Layout")
local AppState = require("AppState")
local setbox = require("SetBox")

local AGC = {}
AGC.__index = AGC

-- Default rules for the AGC widget (very low priority)
setbox.rule {
    id = "agc-defaults",
    tags = {"widget.AGCFrame"},
    priority = -1000,
    apply = {
        title = "AGC",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        buttonHeight = 18,
        buttonGap = 4,
        labelOff = "Off",
        labelSlow = "Slow",
        labelMed = "Med",
        labelFast = "Fast",
    }
}

function AGC.new(state)
    local self = setmetatable({}, AGC)
    self.state = state
    
    -- Initialize widget instances
    self.titleLabel = ui.Label.new()
    self.modeButtons = {
        off  = ui.Button.new({ onClick = function() AppState.set("agcMode", 0) end }),
        slow = ui.Button.new({ onClick = function() AppState.set("agcMode", 3) end }),
        med  = ui.Button.new({ onClick = function() AppState.set("agcMode", 2) end }),
        fast = ui.Button.new({ onClick = function() AppState.set("agcMode", 1) end })
    }
    return self
end

function AGC:draw(id, x, y, w, h)
    local lwc = setbox.newContext({"widget.Panel", "widget.AGCFrame", "id." .. id})
    
    -- Background and border
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end
    
    -- Title
    self.titleLabel:draw(id .. "-title", x + 12, y + 8, lwc)
    
    local padding = lwc:getNumber("padding")
    local topMargin = lwc:getNumber("topMargin")
    local regionW = w - padding * 2
    layout.setRegion(x + padding, y + topMargin, regionW, h - topMargin - padding, id)
    
    local state = self.state
    local bH = lwc:getNumber("buttonHeight")
    local bG = lwc:getNumber("buttonGap")
    
    local modes = {
        { id = "off",  label = lwc:getString("labelOff"),  val = 0 },
        { id = "slow", label = lwc:getString("labelSlow"), val = 3 },
        { id = "med",  label = lwc:getString("labelMed"),  val = 2 },
        { id = "fast", label = lwc:getString("labelFast"), val = 1 }
    }
    
    local buttonW = (regionW - (bG * (#modes - 1))) / #modes
    
    layout.beginHorizontal(0)
    for i, m in ipairs(modes) do
        local bx, by = layout.reserveSpace(buttonW, bH)
        local btn = self.modeButtons[m.id]
        
        -- Use a closure to get label from rules dynamically
        btn.getText = function() return m.label end
        
        -- Add active state tag if matches current mode
        local activeTags = (state.agcMode == m.val) and {"state.Active"} or {}
        
        btn:draw(id .. "-mode-" .. m.id, bx, by, buttonW, bH, activeTags, lwc)
        
        if i < #modes then layout.space(bG) end
    end
    layout.endHorizontal()
    
    layout.endRegion()
end

return AGC
