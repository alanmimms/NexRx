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
    self.editing = false
    self.entryText = ""
    self.cursor = 0
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

function FrequencyDisplay:onEvent(event)
    if event.type == "mouseWheel" then
        local step = 100 -- Default 100 Hz
        if isCtrlDown and isCtrlDown() then step = 10000
        elseif isShiftDown and isShiftDown() then step = 100000 end
        
        local current = self.valueObs:get()
        self.valueObs:set(current + event.delta * step)
        return true
    end

    if event.type == "key" and event.isDown and not self.editing then
        local key = event.key
        local step = 100 -- Default 100 Hz
        if isCtrlDown and isCtrlDown() then step = 10000
        elseif isShiftDown and isShiftDown() then step = 100000 end
        
        local delta = 0
        if key == "RIGHT" or key == "UP" then delta = step
        elseif key == "LEFT" or key == "DOWN" then delta = -step end
        
        if delta ~= 0 then
            local current = self.valueObs:get()
            self.valueObs:set(current + delta)
            return true
        end
    end

    if event.type == "textInput" then
        local text = event.text:upper()
        if not self.editing then
            if text:match("[%dF]") then
                self.editing = true
                self.entryText = text:match("%d") or ""
                self.cursor = #self.entryText
                events.addModeTag("state.FreqEntryMode")
                return true
            end
        else
            if text:match("[%d.,]") then
                self.entryText = self.entryText .. text
                self.cursor = #self.entryText
                return true
            elseif text == "M" or text == "K" then
                local val = tonumber(self.entryText)
                if val then
                    if text == "M" then val = val * 1e6
                    elseif text == "K" then val = val * 1e3 end
                    if self.valueObs and self.valueObs.set then self.valueObs:set(val) end
                end
                self:cancelEdit()
                return true
            end
        end
    elseif event.type == "key" and event.isDown then
        local key = event.key
        if not self.editing then
            -- Start editing on digit or 'F'
            if key:match("^%d$") or key == "F" then
                self.editing = true
                self.entryText = key:match("^%d$") and key or ""
                self.cursor = #self.entryText
                events.addModeTag("state.FreqEntryMode")
                return true
            end
        else
            -- Already editing
            if key:match("^%d$") or key == "PERIOD" or key == "COMMA" then
                local char = key:match("^%d$") or (key == "PERIOD" and "." or ",")
                self.entryText = self.entryText .. char
                self.cursor = #self.entryText
                return true
            elseif key == "M" or key == "K" then
                -- Units (case-insensitive in name but getName might return M/K)
                local val = tonumber(self.entryText)
                if val then
                    if key == "M" then val = val * 1e6
                    elseif key == "K" then val = val * 1e3 end
                    if self.valueObs and self.valueObs.set then self.valueObs:set(val) end
                end
                self:cancelEdit()
                return true
            elseif key == "BACKSPACE" then
                if #self.entryText > 0 then
                    self.entryText = self.entryText:sub(1, -2)
                    self.cursor = #self.entryText
                else
                    self:cancelEdit()
                end
                return true
            elseif key == "ENTER" or key == "RETURN" then
                local val = tonumber(self.entryText)
                if val then
                    -- Heuristic: < 30000 assume kHz, < 100 assume MHz?
                    -- Actually better to just use entered value or unit keys.
                    -- User said: "Heuristic < 1000 = MHz" in AppController, let's keep consistency
                    if val < 1000 then val = val * 1e6 end
                    if self.valueObs and self.valueObs.set then self.valueObs:set(val) end
                end
                self:confirmEdit()
                return true
            elseif key == "ESCAPE" or key == "ESC" then
                self:cancelEdit()
                return true
            elseif key == "LEFT" or key == "RIGHT" or key == "UP" or key == "DOWN" then
                -- Arrow keys are also allowed while editing (though currently just ignored)
                -- but we should return true to consume them if we are in entry mode
                return true
            end
        end
    elseif event.type == "mouseButton" and event.isDown and event.button == "LEFT" then
        if not self.editing then
            self.editing = true
            self.entryText = ""
            self.cursor = 0
            events.addModeTag("state.FreqEntryMode")
            return true
        end
    end

    -- Fallback to rules (like vfo_control for wheel)
    return Widget.onEvent(self, event)
end

function FrequencyDisplay:confirmEdit()
    self.editing = false
    self.entryText = ""
    events.removeModeTag("state.FreqEntryMode")
end

function FrequencyDisplay:cancelEdit()
    self.editing = false
    self.entryText = ""
    events.removeModeTag("state.FreqEntryMode")
end

function FrequencyDisplay:drawSelf(tags)
    local id, w, h = self.id, self.props.w, self.props.h
    local widgetTags = {"widget.FrequencyDisplay", "id." .. id}
    if self.tags then
        for _, t in ipairs(self.tags) do table.insert(widgetTags, t) end
    end
    
    local isEditing = self.editing
    if isEditing then
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
        text = self.entryText
    else
        text = formatFreq(frequency, lwc)
    end

    local tw = System.measureText(text, 20)
    local tx = (w - tw) / 2
    local ty = (h - 20) / 2
    System.drawText(text, tx, ty, 20, {fgR, fgG, fgB, alpha})

    -- Draw Cursor if editing
    if isEditing then
        local blink = (os.clock() % 1.0) < 0.5
        if blink then
            local cursorOffset = System.measureText(text:sub(1, self.cursor), 20)
            local cursorX = tx + cursorOffset
            System.drawRect(cursorX, ty, 2, 20, {fgR, fgG, fgB, alpha})
        end
    end
end

return FrequencyDisplay
