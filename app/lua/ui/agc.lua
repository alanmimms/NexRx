--[[
  AGC Widget Module
  Modular UI component for Automatic Gain Control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

local AGC = {}
AGC.__index = AGC

function AGC.new(state)
    local self = setmetatable({}, AGC)
    self.state = state
    return self
end

function AGC:draw(id, x, y, w, h)
    local style = theme.getStyle({"widget.Panel", "widget.AGCFrame"})
    
    drawRoundedRect(x, y, w, h, style.borderRadius or 6, style.bgR, style.bgG, style.bgB, 1.0)
    drawRectOutline(x, y, w, h, style.borderR, style.borderG, style.borderB, 1.0, 1)
    
    ui.label(id .. "-title", x + 12, y + 8, "AGC", {"Title"})
    
    local padding = 12
    local topMargin = 32
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    
    -- AGC Mode Buttons (Off, Slow, Med, Fast)
    -- We'll lay them out in a 4-column single row for compactness
    local modes = {"Off", "Slow", "Med", "Fast"}
    local buttonW = (w - padding * 2 - (3 * 4)) / 4
    
    layout.beginHorizontal(4)
    for i, m in ipairs(modes) do
        local bx, by = layout.reserveSpace(buttonW, 24)
        local buttonTags = {"AgcMode"}
        
        -- Map i to state value: 1=Off, 2=Slow, 3=Med, 4=Fast
        -- In AppState/HW: 0=Off, 1=Fast, 2=Med, 3=Slow
        -- Let's stick to the HW mapping for state: 0=Off, 1=Fast, 2=Med, 3=Slow
        local hwModeMap = {0, 3, 2, 1}
        local currentMode = hwModeMap[i]
        
        if state.agcMode == currentMode then 
            table.insert(buttonTags, "state.Active") 
        end
        
        if ui.button(id .. "-mode-" .. m:lower(), m, bx, by, buttonW, 24, buttonTags) then
            state.agcMode = currentMode
        end
    end
    layout.endHorizontal()
    
    layout.endRegion()
end

return AGC
