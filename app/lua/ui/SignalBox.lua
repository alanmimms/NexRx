--[[
  SignalBox Widget
  Visual representation of a selected frequency range in the spectrum.
  A proper Widget that handles its own dragging and rendering.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")
local Model = require("Model")

local SignalBox = Widget.mkType("SignalBox", Widget)

-- Default rules for SignalBox widget
setbox.rule {
    id = "signalbox-defaults",
    tags = {"widget.SignalBox"},
    priority = -1000,
    apply = {
        background = "#facc15",
        foreground = "#ffffff",
        border = "#facc15",
        borderWidth = 1,
        opacity = 0.3,
        selectedOpacity = 0.6,
        selectedBorderWidth = 3,
        ghostOpacity = 0.2,
        tagHeight = 20,
        tagWidth = 40,
    }
}

function SignalBox:init(def)
    Widget.init(self, def)
    self.boxIndex = def.index or 1
    self.dragging = false
    -- Capture start state for stable dragging
    self.dragStartMouseX = 0
    self.dragStartBoxX = 0
end

function SignalBox:onEvent(event)
    local box = Model.signalBoxes:peek()[self.boxIndex]
    if not box then return false end

    if event.type == "mouseButton" and event.button == "LEFT" then
        if event.isDown then
            self.dragging = true
            -- Use global gx for stable dragging
            self.dragStartMouseX = event.gx
            self.dragStartBoxX = self.props.x
            
            state.setActive(self.id)
            Model.selectedSignalBoxIndex:set(self.boxIndex)
            return true
        else
            self.dragging = false
            state.setActive(nil)
            return true
        end
    elseif event.type == "mouseMotion" and self.dragging then
        local totalDeltaPx = event.gx - self.dragStartMouseX
        local newBoxX = self.dragStartBoxX + totalDeltaPx
        
        -- Update pixel position immediately for the 'stuck' feel
        self.props.x = newBoxX
        
        -- Update model from the new pixel position
        if self.parent and self.parent.getFreqAtPx then
            -- SignalBox frequency is its center
            local newFreq = self.parent:getFreqAtPx(newBoxX + self.props.w / 2)
            if math.abs(box.frequency - newFreq) > 0.1 then
                box.frequency = newFreq
                -- Note: AppController watches box.frequency and will sync to HW
            end
        end
        return true
    end

    return Widget.onEvent(self, event)
end

function SignalBox:drawSelf()
    local w, h = self.props.w, self.props.h
    local lwc = self.lwc
    
    local isGhost = lwc:hasTag("state.Ghost")
    local isSelected = (Model.selectedSignalBoxIndex:get() == self.boxIndex)
    
    -- Style resolution
    local bgR, bgG, bgB = state.hexToRgb(lwc:optString("background", "#facc15"))
    local fgR, fgG, fgB = state.hexToRgb(lwc:optString("foreground", "#ffffff"))
    local bR, bG, bB = state.hexToRgb(lwc:optString("border", "#facc15"))
    
    local alpha = lwc:optNumber("opacity", 0.3)
    local bWidth = lwc:optNumber("borderWidth", 1)
    
    if isSelected then
        alpha = lwc:optNumber("selectedOpacity", 0.6)
        bWidth = lwc:optNumber("selectedBorderWidth", 3)
    elseif isGhost then
        alpha = lwc:optNumber("ghostOpacity", 0.2)
    end
    
    -- 1. Draw the translucent box
    System.drawRect(0, 0, w, h, {bgR, bgG, bgB, alpha})
    
    -- 2. Draw border
    if bWidth > 0 then
        System.drawLine(0, 0, 0, h, bWidth, {bR, bG, bB, alpha})
        System.drawLine(w, 0, w, h, bWidth, {bR, bG, bB, alpha})
        System.drawLine(0, 0, w, 0, bWidth, {bR, bG, bB, alpha})
    end
    
    -- 3. Draw the numeric tag at the bottom
    local box = Model.signalBoxes:peek()[self.boxIndex]
    local label = box and (box.name or tostring(box.mode)) or "?"
    
    local tagH = lwc:optNumber("tagHeight", 20)
    local tagW = lwc:optNumber("tagWidth", 40)
    local tx = (w - tagW) / 2
    local ty = h - tagH
    
    System.drawRoundedRect(tx, ty, tagW, tagH, 4, {bgR, bgG, bgB, 1.0})
    
    local labelW = System.measureText(tostring(label), 14)
    local lineH = 14
    System.drawText(tostring(label), tx + (tagW - labelW) / 2, ty + (tagH - lineH) / 2, 14, {fgR, fgG, fgB, 1.0})
end

return SignalBox
