--[[
  Frequency Display Widget
  Modular UI component for radio frequency readout.
  Style and behavior driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local events = require("Events")
local Widget = require("ui.Widget")

local FrequencyDisplay = Widget.mkType("FrequencyDisplay")

-- Default rules for FrequencyDisplay widget (very low priority)
setbox.rule {
    id = "frequency-display-defaults",
    tags = {"widget.FrequencyDisplay"},
    priority = -1000,
    apply = {
        background = "#1e293b",
        foreground = "#ffffff",
        border = "#3b82f6",
        highlight = "#facc15", -- Yellow highlight for editing/focus
        highlighted = false,
        borderWidth = 1,
        borderRadius = 4,
        opacity = 1.0,
        height = 36,
    }
}

function FrequencyDisplay:init(def)
    Widget.init(self, def)
    self.valueObs = def.valueObs
end

function FrequencyDisplay:calcMetrics()
    if self.metrics.prefW == 0 then self.metrics.prefW = 200 end
    if self.metrics.prefH == 0 then self.metrics.prefH = 40 end
end

local function formatFreq(f_hz, lwc)
    local sep = ","
    if lwc and lwc.optString then
        sep = lwc:optString("locale.thousandsSeparator", ",")
    end
    local s = string.format("%.0f", f_hz)
    local res = ""
    local count = 0
    for i = #s, 1, -1 do
        res = s:sub(i, i) .. res
        count = count + 1
        if count == 3 and i > 1 then
            res = sep .. res
            count = 0
        end
    end
    return res .. " Hz"
end

function FrequencyDisplay:drawSelf(freqEntryText, cursorIdx, tags)
    local id, w, h = self.id, self.props.w, self.props.h
    local widgetTags = {"widget.FrequencyDisplay", "id." .. id}
    if self.tags then
        for _, t in ipairs(self.tags) do table.insert(widgetTags, t) end
    end
    
    local isEditing = (freqEntryText and freqEntryText ~= "")
    local hasModeTag = events.hasModeTag("state.FreqEntryMode")
    
    if isEditing or hasModeTag then
        table.insert(widgetTags, "state.FreqEntryMode")
    end

    if tags then
        for _, t in ipairs(tags) do table.insert(widgetTags, t) end
    end

    if state.isHot(id) then table.insert(widgetTags, "state.Hovered") end
    if state.isActive(id) then table.insert(widgetTags, "state.Active") end

    local lwc = self.lwc
    
    state.registerWidget(id, {x=0, y=0, w=w, h=h}, widgetTags)
    if state.pointInRect(state.mouseX, state.mouseY, 0, 0, w, h) then
        state.setHot(id)
        if state.mouseClicked then state.setActive(id) end
    end

    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#1e293b"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#3b82f6"))
    
    if lwc:optBool("highlighted", false) then
        bR, bG, bB = state.hexToRgb(lwc:optString("highlight", "#facc15"))
    end

    local bWidth = lwc:optNumber("borderWidth", 1)
    local radius = lwc:optNumber("borderRadius", 4)
    local alpha = lwc:optNumber("opacity", 1.0)

    -- Local coords 0,0
    System.drawRoundedRect(0, 0, w, h, radius, {bgR, bgG, bgB, alpha})
    if bWidth > 0 then
        System.drawRectLines(0, 0, w, h, bWidth, {bR, bG, bB, alpha})
    end

    local frequency = 0
    if self.valueObs then
        frequency = self.valueObs:get()
    end
    
    local text = ""
    if isEditing then
        text = freqEntryText
    else
        text = formatFreq(frequency, lwc)
    end

    local tw = System.measureText(text, 20)
    local tx = (w - tw) / 2
    local ty = (h - 20) / 2
    System.drawText(text, tx, ty, 20, {fgR, fgG, fgB, alpha})

    -- Draw Cursor if editing
    if isEditing and cursorIdx then
        local blink = (_G.freqEntryBlink or 0) < 0.5
        if blink then
            local cursorOffset = System.measureText(text:sub(1, cursorIdx), 20)
            local cursorX = tx + cursorOffset
            System.drawRect(cursorX, ty, 2, 20, {fgR, fgG, fgB, alpha})
        end
    end
end

return FrequencyDisplay
