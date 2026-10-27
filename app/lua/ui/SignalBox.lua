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
    def = def or {}
    Widget.init(self, def)
    self.boxIndex = def.index or 1
    self.dragging = false
    self.dragStartMouseX = 0
    self.dragStartBoxX = 0
end

function SignalBox:onEvent(event)
    local box = Model.signalBoxes:peek()[self.boxIndex]
    if not box then return false end

    if event.type == "mouseButton" and event.button == "LEFT" then
        if event.isDown then
            self.dragging = true
            self.dragStartMouseX = event.gx or event.x or 0
            self.dragStartBoxX = self.props.x or 0

            state.setActive(self.id)
            if self.lwc then self.lwc:addTag("state.Active") end
            Model.selectedSignalBoxIndex:set(self.boxIndex)
            return true
        else
            self.dragging = false
            state.setActive(nil)
            if self.lwc then self.lwc:removeTag("state.Active") end
            return true
        end
    elseif event.type == "mouseMotion" and self.dragging then
        local totalDeltaPx = (event.gx or event.x or 0) - self.dragStartMouseX
        local newBoxX = self.dragStartBoxX + totalDeltaPx

        self.props.x = newBoxX

        if self.parent and self.parent.getFreqAtPx then
            local newFreq = self.parent:getFreqAtPx(newBoxX + (self.props.w or 0) / 2)
            if math.abs(box.frequency - newFreq) > 0.1 then
                Model.set("rx.VFO.activeValue", newFreq)
            end
        end
        return true
    end

    return Widget.onEvent(self, event)
end

function SignalBox:drawSelf()
    local w, h = self.props.w or 0, self.props.h or 0
    local isGhost = self:hasTag("state.Ghost")
    local isSelected = (Model.selectedSignalBoxIndex:get() == self.boxIndex)

    if self.lwc then
        self.lwc:setTag("state.Selected", isSelected)
    end

    local bgR, bgG, bgB = state.hexToRgb(self.props.background or "#facc15")
    local fgR, fgG, fgB = state.hexToRgb(self.props.foreground or "#ffffff")
    local bR, bG, bB = state.hexToRgb(self.props.border or "#facc15")

    local alpha = self.props.opacity or 0.3
    local bWidth = self.props.borderWidth or 1

    if isSelected then
        alpha = self.props.selectedOpacity or 0.6
        bWidth = self.props.selectedBorderWidth or 3
    elseif isGhost then
        alpha = self.props.ghostOpacity or 0.2
    end

    System.drawRect(0, 0, w, h, {bgR, bgG, bgB, alpha})

    if bWidth > 0 then
        System.drawLine(0, 0, 0, h, bWidth, {bR, bG, bB, alpha})
        System.drawLine(w, 0, w, h, bWidth, {bR, bG, bB, alpha})
        System.drawLine(0, 0, w, 0, bWidth, {bR, bG, bB, alpha})
    end

    local box = Model.signalBoxes:peek()[self.boxIndex]
    local label = box and (box.name or tostring(box.mode)) or "?"

    local tagH = self.props.tagHeight or 20
    local tagW = self.props.tagWidth or 40
    local tx = (w - tagW) / 2
    local ty = h - tagH

    System.drawRoundedRect(tx, ty, tagW, tagH, 4, {bgR, bgG, bgB, 1.0})

    local labelW = System.measureText(tostring(label), 14)
    local lineH = 14
    System.drawText(tostring(label), tx + (tagW - labelW) / 2, ty + (tagH - lineH) / 2, 14, {fgR, fgG, fgB, 1.0})
end

return SignalBox
