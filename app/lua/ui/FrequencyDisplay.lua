--[[
  Frequency Display Widget
  Modular UI component for radio frequency readout.
  Style and behavior driven by SetBox rules and reactive self.props.
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
        highlight = "#facc15",
        highlighted = false,
        borderWidth = 1,
        borderRadius = 4,
        opacity = 1.0,
        height = 36,
    }
}

function FrequencyDisplay:init(def)
    def = def or {}
    Widget.init(self, def)
    self.valueObs = def.valueObs
    self.editing = false
    self.entryText = ""
    self.cursor = 0
end

function FrequencyDisplay:calcMetrics()
    if (self.props.prefW or 0) == 0 then self.props.prefW = 200 end
    if (self.props.prefH or 0) == 0 then self.props.prefH = 40 end
end

local function formatFreq(fHz, props)
    local sep = (props and props["locale.thousandsSeparator"]) or ","
    local s = string.format("%.0f", fHz or 0)
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

function FrequencyDisplay:beginEdit(initialText)
    self.editing = true
    self.entryText = initialText or ""
    self.cursor = #self.entryText
    if self.lwc then self.lwc:addTag("state.FreqEntryMode") end
    events.addModeTag("state.FreqEntryMode")
end

function FrequencyDisplay:confirmEdit()
    self.editing = false
    self.entryText = ""
    if self.lwc then self.lwc:removeTag("state.FreqEntryMode") end
    events.removeModeTag("state.FreqEntryMode")
end

function FrequencyDisplay:cancelEdit()
    self.editing = false
    self.entryText = ""
    if self.lwc then self.lwc:removeTag("state.FreqEntryMode") end
    events.removeModeTag("state.FreqEntryMode")
end

function FrequencyDisplay:onEvent(event)
    if event.type == "mouseWheel" then
        if self.editing then
            self:cancelEdit()
        end
        local step = 100
        if isCtrlDown and isCtrlDown() then step = 10000
        elseif isShiftDown and isShiftDown() then step = 100000 end

        if self.valueObs then
            local current = self.valueObs:get()
            self.valueObs:set(current + (event.delta or 0) * step)
        end
        return true
    end

    if event.type == "key" and event.isDown and not self.editing then
        local key = event.key
        local step = 100
        if isCtrlDown and isCtrlDown() then step = 10000
        elseif isShiftDown and isShiftDown() then step = 100000 end

        local delta = 0
        if key == "RIGHT" or key == "UP" then delta = step
        elseif key == "LEFT" or key == "DOWN" then delta = -step end

        if delta ~= 0 and self.valueObs then
            local current = self.valueObs:get()
            self.valueObs:set(current + delta)
            return true
        end
    end

    if event.type == "textInput" then
        local text = (event.text or ""):upper()
        if not self.editing then
            if text:match("[%dF]") then
                self:beginEdit(text:match("%d") or "")
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
            if key:match("^%d$") or key == "F" then
                self:beginEdit(key:match("^%d$") and key or "")
                return true
            end
        else
            if key:match("^%d$") or key == "PERIOD" or key == "COMMA" then
                local char = key:match("^%d$") or (key == "PERIOD" and "." or ",")
                self.entryText = self.entryText .. char
                self.cursor = #self.entryText
                return true
            elseif key == "M" or key == "K" then
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
                    if val < 1000 then val = val * 1e6 end
                    if self.valueObs and self.valueObs.set then self.valueObs:set(val) end
                end
                self:confirmEdit()
                return true
            elseif key == "ESCAPE" or key == "ESC" then
                self:cancelEdit()
                return true
            elseif key == "LEFT" or key == "RIGHT" or key == "UP" or key == "DOWN" then
                return true
            end
        end
    elseif event.type == "mouseButton" and event.isDown and event.button == "LEFT" then
        if not self.editing then
            self:beginEdit("")
            return true
        end
    end

    return Widget.onEvent(self, event)
end

function FrequencyDisplay:onBlur()
    if self.editing then
        self:cancelEdit()
    end
end

function FrequencyDisplay:drawSelf()
    if self.editing and not self.focused then
        self:cancelEdit()
    end
    local w, h = self.props.w or 0, self.props.h or 0
    local isEditing = self.editing

    local bgR, bgG, bgB, bgA = state.hexToRgb(self.props.background or "#1e293b")
    local fgR, fgG, fgB, fgA = state.hexToRgb(self.props.foreground or "#ffffff")
    local bR, bG, bB, bA = state.hexToRgb(self.props.border or "#3b82f6")

    if self.props.highlighted or isEditing then
        bR, bG, bB, bA = state.hexToRgb(self.props.highlight or "#facc15")
    end

    local bWidth = self.props.borderWidth or 1
    local radius = self.props.borderRadius or 4
    local alpha = self.props.opacity or 1.0

    System.drawRoundedRect(0, 0, w, h, radius, {bgR, bgG, bgB, (bgA or 1.0) * alpha})
    if bWidth > 0 then
        System.drawRectLines(0, 0, w, h, bWidth, {bR, bG, bB, (bA or 1.0) * alpha})
    end

    local frequency = 0
    if self.valueObs then
        frequency = self.valueObs:get()
    elseif self.props.value ~= nil then
        frequency = self.props.value
    end

    local text = ""
    if isEditing then
        text = self.entryText
    else
        text = formatFreq(frequency, self.props)
    end

    local tw = System.measureText(text, 20)
    local tx = (w - tw) / 2
    local ty = (h - 20) / 2
    System.drawText(text, tx, ty, 20, {fgR, fgG, fgB, (fgA or 1.0) * alpha})

    if isEditing then
        local blink = (os.clock() % 1.0) < 0.5
        if blink then
            local cursorOffset = System.measureText(text:sub(1, self.cursor), 20)
            local cursorX = tx + cursorOffset
            System.drawRect(cursorX, ty, 2, 20, {fgR, fgG, fgB, (fgA or 1.0) * alpha})
        end
    end
end

return FrequencyDisplay
