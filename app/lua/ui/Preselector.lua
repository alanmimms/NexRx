--[[
  Preselector Widget Module
  Modular UI component for RX preselector control.
  Style and behavior driven entirely by SetBox rules.
]]

local ui = require("ui.Widgets")
local layout = require("ui.Layout")
local setbox = require("SetBox")
local Model = require("Model")

local Preselector = {}
Preselector.__index = Preselector

-- Default rules for the Preselector widget (very low priority)
setbox.rule {
    id = "preselector-defaults",
    tags = {"widget.PreselectorFrame"},
    priority = -1000,
    apply = {
        title = "PRESELECTOR",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        labelAuto = "Auto-tune",
        gridRowHeight = 20,
        gridRowGap = 4,
        gridColWidth = 60,
        gridColGap = 8,
        gridCols = 4,
    }
}

function Preselector.new(props)
    local self = setmetatable({}, Preselector)
    self.preselector = props.preselector -- Model.preselector
    
    local cbNames = {"L", "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "C10"}

    -- Initialize widget instances
    self.titleLabel = ui.Label.new()
    self.autoCheckbox = ui.Checkbox.new({
        onToggle = function(val) Model.set("preselector.auto", val) end
    })
    
    self.checkboxes = {}

    for k, name in ipairs(cbNames) do
        if name == "L" then
            self.checkboxes[k] = ui.Checkbox.new({
                getText = function() return "L1" end,
                onToggle = function(val) 
                    Model.set("preselector.auto", false)
                    Model.set("preselector.L", val) 
                end,
            })
        else
            local bitIndex = tonumber(name:match("C(%d+)"))
            self.checkboxes[k] = ui.Checkbox.new({
                getText = function() return name end,
                onToggle = function(val)
                    Model.set("preselector.auto", false)
                    local currentMask = self.preselector.capMask:peek()
                    local bit = 2 ^ bitIndex
                    local newMask = val and (currentMask | bit) or (currentMask & ~bit)
                    Model.set("preselector.capMask", newMask)
                end,
            })
        end
        self.checkboxes[k].name = name
    end
    
    return self
end

function Preselector:draw(id, x, y, w, h)
    local lwc = setbox.newContext({"widget.Panel", "widget.PreselectorFrame", "id." .. id})
    
    -- 1. Draw surrounding frame
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    if bWidth > 0 then
        drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    end
    
    -- 2. Draw Frame Label
    self.titleLabel:draw(id .. "-title", x + 12, y + 8, lwc)
    
    -- 3. Setup inner layout region
    local padding = lwc:getNumber("padding")
    local topMargin = lwc:getNumber("topMargin")
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local cx, cy
    
    -- Row 1: Auto-tune
    local rowH = lwc:getNumber("gridRowHeight")
    local rowGap = lwc:getNumber("gridRowGap")
    cx, cy = layout.getCursor()
    self.autoCheckbox.getText = function() return lwc:getString("labelAuto") end
    self.autoCheckbox:draw(id .. "-auto", cx, cy, self.preselector.auto:get(), lwc)
    layout.newLine(rowH + rowGap)
    
    -- Parameterized Component Grid
    local colW = lwc:getNumber("gridColWidth")
    local colGap = lwc:getNumber("gridColGap")
    local cols = lwc:getNumber("gridCols")
    
    -- Draw items in a uniform grid
    local mask = self.preselector.capMask:get()
    local L = self.preselector.L:get()

    for i, cb in ipairs(self.checkboxes) do
        local gridIdx = (i - 1) % cols
        if gridIdx == 0 then layout.beginHorizontal(0) end
        
        local bx, by = layout.reserveSpace(colW, rowH)
        local checked = false
        if cb.name == "L" then
            checked = L
        else
            local bitIndex = tonumber(cb.name:match("C(%d+)"))
            checked = (mask & (2 ^ bitIndex)) ~= 0
        end

        cb:draw(id .. "-" .. cb.name, bx, by, checked, lwc)
        
        if gridIdx < cols - 1 and i < #self.checkboxes then 
            layout.space(colGap) 
        end
        
        if gridIdx == cols - 1 or i == #self.checkboxes then 
            layout.endHorizontal() 
            layout.newLine(rowH + rowGap)
        end
    end
    
    layout.endRegion()
end

return Preselector
