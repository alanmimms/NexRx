--[[
  Preselector Widget Module
  Modular UI component for RX preselector control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local AppState = require("app_state")

local Preselector = {}
Preselector.__index = Preselector

-- Default rules for the Preselector widget (low priority)
setbox.rule {
    id = "preselector-defaults",
    tags = {"widget.PreselectorFrame"},
    priority = -50,
    apply = {
        title = "PRESELECTOR",
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
        borderWidth = 1,
        opacity = 1.0,
        padding = 12,
        topMargin = 32,
        gridRowHeight = 18,
        gridColWidth = 55,
        gridColGap = 10,
        gridRowGap = 0,
        gridCols = 4,
        labelAuto = "Auto-tune",
        labelL1 = "L1",
    }
}

function Preselector.new(state)
    local self = setmetatable({}, Preselector)
    self.state = state
    return self
end

function Preselector:draw(id, x, y, w, h)
    -- Create Local Widget Context (LWC)
    local lwc = setbox.newContext({"widget.Panel", "widget.PreselectorFrame", "id." .. id})
    
    -- Properties fetched individually from LWC
    local bgR, bgG, bgB = ui.hexToRgb(lwc:getString("background"))
    local bR, bG, bB = ui.hexToRgb(lwc:getString("border"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local bWidth = lwc:getNumber("borderWidth")
    
    -- 1. Draw surrounding frame
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    
    -- 2. Draw Frame Label
    ui.label(id .. "-title", x + 12, y + 8, lwc:getString("title"), {"Title"}, lwc)
    
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
    if ui.checkbox(id .. "-auto", lwc:getString("labelAuto"), cx, cy, state.preselectorAuto, {"PreselAuto"}, "preselectorAuto", lwc) then
        AppState.set("preselectorAuto", not state.preselectorAuto)
    end
    layout.newLine(rowH + rowGap)
    
    -- Parameterized Component Grid
    local colW = lwc:getNumber("gridColWidth")
    local colGap = lwc:getNumber("gridColGap")
    local cols = lwc:getNumber("gridCols")
    
    -- Define the set of grid items (1 L + 11 Cs)
    local items = {
        { id = "l1", label = lwc:getString("labelL1"), prop = "preselL1" }
    }
    for i = 0, 10 do
        table.insert(items, { id = "c" .. i, label = "C" .. i, prop = "preselC" .. i })
    end
    
    -- Draw items in a uniform grid
    for i, item in ipairs(items) do
        local gridIdx = (i - 1) % cols
        if gridIdx == 0 then layout.beginHorizontal(cols) end
        
        local bx, by = layout.reserveSpace(colW, rowH)
        if ui.checkbox(id .. "-" .. item.id, item.label, bx, by, state[item.prop], {"PreselToggle"}, item.prop, lwc) then
            AppState.set(item.prop, not state[item.prop])
        end
        
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
