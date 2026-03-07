--[[
  Preselector Widget Module
  Modular UI component for RX preselector control.
]]

local ui = require("ui.widgets")
local layout = require("ui.layout")
local theme = require("ui.theme")

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
        gridRowHeight = 20,
        gridColWidth = 55,
        gridColGap = 10,
        gridRowGap = 1,
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
    local prevTags = setbox.getActiveTags()
    setbox.setActiveTags({"widget.Panel", "widget.PreselectorFrame", "id." .. id})
    
    -- Properties fetched individually from SetBox (defaults provided by preselector-defaults rule)
    local bgR, bgG, bgB = theme.hexToRgb(setbox.getString("background", "#242d42"))
    local bR, bG, bB = theme.hexToRgb(setbox.getString("border", "#3b82f6"))
    local radius = setbox.getNumber("borderRadius", 8)
    local alpha = setbox.getNumber("opacity", 1.0)
    local bWidth = setbox.getNumber("borderWidth", 1)
    
    -- 1. Draw surrounding frame
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    drawRectOutline(x, y, w, h, bR, bG, bB, alpha, bWidth)
    
    -- 2. Draw Frame Label
    local title = setbox.getString("title", "PRESELECTOR")
    ui.label(id .. "-title", x + 12, y + 8, title, {"Title"})
    
    -- 3. Setup inner layout region
    local padding = setbox.getNumber("padding", 12)
    local topMargin = setbox.getNumber("topMargin", 32)
    layout.setRegion(x + padding, y + topMargin, w - padding * 2, h - topMargin - padding, id)
    
    local state = self.state
    local cx, cy
    
    -- Row 1: Auto-tune
    local rowH = setbox.getNumber("gridRowHeight", 20)
    local rowGap = setbox.getNumber("gridRowGap", 1)
    cx, cy = layout.getCursor()
    ui.checkbox(id .. "-auto", setbox.getString("labelAuto", "Auto-tune"), cx, cy, state.preselectorAuto, {"PreselAuto"}, "preselectorAuto")
    layout.newLine(rowH + rowGap + 4)
    
    -- Compact Grid for Inductor and Capacitors
    local colW = setbox.getNumber("gridColWidth", 55)
    local colGap = setbox.getNumber("gridColGap", 10)
    
    -- Row 1: L1 and C0-C2
    layout.beginHorizontal(4)
    local bx, by = layout.reserveSpace(colW, rowH)
    ui.checkbox(id .. "-l1", setbox.getString("labelL1", "L1"), bx, by, state.preselL1, {"PreselToggle"}, "preselL1")
    layout.space(colGap)
    
    for i = 0, 2 do
        local cid = "preselC" .. i
        bx, by = layout.reserveSpace(colW, rowH)
        ui.checkbox(id .. "-" .. cid, "C" .. i, bx, by, state[cid], {"PreselToggle"}, cid)
        if i < 2 then layout.space(colGap) end
    end
    layout.endHorizontal()
    layout.newLine(rowH + rowGap)
    
    -- Remaining Capacitors C3-C10 (4 per row)
    for i = 3, 10 do
        local gridIdx = (i - 3) % 4
        if gridIdx == 0 then layout.beginHorizontal(4) end
        
        local cid = "preselC" .. i
        bx, by = layout.reserveSpace(colW, rowH)
        ui.checkbox(id .. "-" .. cid, "C" .. i, bx, by, state[cid], {"PreselToggle"}, cid)
        
        if gridIdx < 3 and i < 10 then layout.space(colGap) end
        
        if gridIdx == 3 or i == 10 then 
            layout.endHorizontal() 
            layout.newLine(rowH + rowGap)
        end
    end
    
    layout.endRegion()
    setbox.setActiveTags(prevTags)
end

return Preselector
