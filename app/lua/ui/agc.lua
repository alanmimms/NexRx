--[[
  AGC Widget Module
  Modular UI component for Automatic Gain Control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local AppState = require("app_state")

local AGC = {}
AGC.__index = AGC

-- Default rules for the AGC widget
setbox.rule {
    id = "agc-defaults",
    tags = {"widget.AGCFrame"},
    priority = -50,
    apply = {
        title = "AGC",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        buttonHeight = 20,
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
    return self
end

function AGC:draw(id, x, y, w, h)
    local lwc = setbox.newContext({"widget.Panel", "widget.AGCFrame", "id." .. id})
    
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
    local regionW = w - padding * 2
    layout.setRegion(x + padding, y + topMargin, regionW, h - topMargin - padding, id)
    
    local state = self.state
    local bH = lwc:getNumber("buttonHeight")
    local bG = lwc:getNumber("buttonGap")
    
    -- Compact AGC Mode Buttons (Off, Slow, Med, Fast)
    local modes = {
        { label = lwc:getString("labelOff"),  val = 0 },
        { label = lwc:getString("labelSlow"), val = 3 },
        { label = lwc:getString("labelMed"),  val = 2 },
        { label = lwc:getString("labelFast"), val = 1 }
    }
    
    local buttonW = (regionW - (bG * (#modes - 1))) / #modes
    
    layout.beginHorizontal(0)
    for i, m in ipairs(modes) do
        local bx, by = layout.reserveSpace(buttonW, bH)
        local buttonTags = {"AgcMode"}
        
        if state.agcMode == m.val then 
            table.insert(buttonTags, "state.Active") 
        end
        
        if ui.button(id .. "-mode-" .. m.label:lower(), m.label, bx, by, buttonW, bH, buttonTags, lwc) then
            AppState.set("agcMode", m.val)
        end
        
        if i < #modes then layout.space(bG) end
    end
    layout.endHorizontal()
    
    layout.endRegion()
end

return AGC
