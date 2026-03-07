--[[
  Preselector Widget Module
  Modular UI component for RX preselector control.
  Style and behavior driven entirely by SetBox rules.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local AppState = require("app_state")
local setbox = require("setbox")

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
        labelL1 = "L1 (220nH)",
        gridRowHeight = 20,
        gridRowGap = 4,
        gridColWidth = 60,
        gridColGap = 8,
        gridCols = 4,
    }
}

function Preselector.new(state)
    local self = setmetatable({}, Preselector)
    self.state = state
    
    -- Initialize widget instances
    self.titleLabel = ui.Label.new()
    self.autoCheckbox = ui.Checkbox.new({
        onToggle = function(val) AppState.set("preselectorAuto", val) end
    })
    
    self.l1Checkbox = ui.Checkbox.new({
        onToggle = function(val) AppState.set("preselL1", val) end
    })
    
    self.cCheckboxes = {}
    for i = 0, 10 do
        local prop = "preselC" .. i
        self.cCheckboxes[i] = ui.Checkbox.new({
            onToggle = function(val) AppState.set(prop, val) end
        })
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
    
    local state = self.state
    local cx, cy
    
    -- Row 1: Auto-tune
    local rowH = lwc:getNumber("gridRowHeight")
    local rowGap = lwc:getNumber("gridRowGap")
    cx, cy = layout.getCursor()
    self.autoCheckbox.getText = function() return lwc:getString("labelAuto") end
    self.autoCheckbox:draw(id .. "-auto", cx, cy, state.preselectorAuto, lwc)
    layout.newLine(rowH + rowGap)
    
    -- Parameterized Component Grid
    local colW = lwc:getNumber("gridColWidth")
    local colGap = lwc:getNumber("gridColGap")
    local cols = lwc:getNumber("gridCols")
    
    -- Define the set of grid items (1 L + 11 Cs)
    local items = {
        { id = "l1", label = lwc:getString("labelL1"), prop = "preselL1", widget = self.l1Checkbox }
    }
    for i = 0, 10 do
        table.insert(items, { id = "c" .. i, label = "C" .. i, prop = "preselC" .. i, widget = self.cCheckboxes[i] })
    end
    
    -- Draw items in a uniform grid
    for i, item in ipairs(items) do
        local gridIdx = (i - 1) % cols
        if gridIdx == 0 then layout.beginHorizontal(0) end
        
        local bx, by = layout.reserveSpace(colW, rowH)
        -- Overload label for this specific item
        item.widget.getText = function() return item.label end
        item.widget:draw(id .. "-" .. item.id, bx, by, state[item.prop], lwc)
        
        if gridIdx < cols - 1 and i < #items then 
            layout.space(colGap) 
        end
        
        if gridIdx == cols - 1 or i == #items then 
            layout.endHorizontal() 
            layout.newLine(rowH + rowGap)
        end
    end
    
    layout.endRegion()
end

return Preselector
