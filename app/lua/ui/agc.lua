--[[
  AGC Widget Module
  Modular UI component for Automatic Gain Control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

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
        buttonHeight = 24,
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
    local prevTags = setbox.getActiveTags()
    setbox.setActiveTags({"widget.Panel", "widget.AGCFrame", "id." .. id})
    
    local bgR, bgG, bgB = theme.hexToRgb(setbox.getString("background", "#242d42"))
    local bR, bG, bB = theme.hexToRgb(setbox.getString("border", "#3b82f6"))
    local radius = setbox.getNumber("borderRadius", 8)
    local alpha = setbox.getNumber("opacity", 1.0)
    local bWidth = setbox.getNumber("borderWidth", 1)
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    
    ui.label(id .. "-title", x + 12, y + 8, setbox.getString("title", "AGC"), {"Title"})
    
    local padding = setbox.getNumber("padding", 12)
    local topMargin = setbox.getNumber("topMargin", 32)
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    local bH = setbox.getNumber("buttonHeight", 24)
    local bG = setbox.getNumber("buttonGap", 4)
    
    -- Compact AGC Mode Buttons (Off, Slow, Med, Fast)
    local modes = {
        { label = setbox.getString("labelOff", "Off"),  val = 0 },
        { label = setbox.getString("labelSlow", "Slow"), val = 3 },
        { label = setbox.getString("labelMed", "Med"),  val = 2 },
        { label = setbox.getString("labelFast", "Fast"), val = 1 }
    }
    
    local buttonW = (w - padding * 2 - (bG * (#modes - 1))) / #modes
    
    layout.beginHorizontal(#modes)
    for i, m in ipairs(modes) do
        local bx, by = layout.reserveSpace(buttonW, bH)
        local buttonTags = {"AgcMode"}
        
        if state.agcMode == m.val then 
            table.insert(buttonTags, "state.Active") 
        end
        
        if ui.button(id .. "-mode-" .. m.label:lower(), m.label, bx, by, buttonW, bH, buttonTags) then
            state.agcMode = m.val
        end
        
        if i < #modes then layout.space(bG) end
    end
    layout.endHorizontal()
    
    layout.endRegion()
    setbox.setActiveTags(prevTags)
end

return AGC
