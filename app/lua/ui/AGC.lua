--[[
  AGC Widget Module
  Modular UI component for Automatic Gain Control.
  Style and layout driven entirely by SetBox rules.
]]

local ui = require("ui.Widgets")
local layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")

local AGC = {}
AGC.__index = AGC

-- Default rules for the AGC widget (very low priority)
setbox.rule {
    id = "agc-defaults",
    tags = {"widget.AGCFrame"},
    priority = -1000,
    apply = {
        title = "AGC",
        text = "AGC",
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

function AGC.new(props)
    local self = setmetatable({}, AGC)
    self.AGC = props.AGC -- Model.rx.AGC
    
    -- Initialize widget instances
    self.titleLabel = ui.Label.new({ 
        getText = function(lwc) 
            return lwc and lwc:getString("title") or "AGC" 
        end 
    })
    self.modeButtons = {
        off  = ui.Button.new({ onClick = function() Model.set("rx.AGC.mode", 0) end }),
        slow = ui.Button.new({ onClick = function() Model.set("rx.AGC.mode", 3) end }),
        med  = ui.Button.new({ onClick = function() Model.set("rx.AGC.mode", 2) end }),
        fast = ui.Button.new({ onClick = function() Model.set("rx.AGC.mode", 1) end })
    }
    self.bypassCheck = ui.Checkbox.new({
        getText = function() return "Bypass Matrix" end,
        onToggle = function(v) Model.set("rx.DSP.matrixBypass", v) end
    })
    self.lmsCheck = ui.Checkbox.new({
        getText = function() return "LMS Alignment" end,
        onToggle = function(v) Model.set("rx.DSP.lmsEnabled", v) end
    })
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
    self.titleLabel.getText = function() return lwc:getString("title") or "AGC" end
    self.titleLabel:draw(id .. "-title", x + 12, y + 8, lwc)
    
    local padding = lwc:getNumber("padding")
    local topMargin = lwc:getNumber("topMargin")
    local regionW = w - padding * 2
    layout.setRegion(x + padding, y + topMargin, regionW, h - topMargin - padding, id)
    
    local currentMode = self.AGC.mode:get()
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
        local activeTags = (currentMode == m.val) and {"state.Active"} or {}
        
        btn:draw(id .. "-mode-" .. m.id, bx, by, buttonW, bH, activeTags, lwc)
        
        if i < #modes then layout.space(bG) end
    end
    layout.endHorizontal()
    
    layout.newLine(24)
    local cx, cy = layout.getCursor()
    self.bypassCheck:draw(id .. "-matrix-bypass", cx, cy, Model.rx.DSP.matrixBypass:get(), lwc)
    layout.newLine(24)
    cx, cy = layout.getCursor()
    self.lmsCheck:draw(id .. "-lms-enable", cx, cy, Model.rx.DSP.lmsEnabled:get(), lwc)

    layout.endRegion()
end

return AGC
