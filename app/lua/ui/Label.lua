--[[
  Label Widget
  Modular UI component for text display.
  Everything comes from SetBox rules.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local TextMixin = require("ui.TextMixin")

local Label = {}
Label.__index = Label

-- Default rules for Label widget (very low priority)
setbox.rule {
    id = "label-defaults",
    tags = {"widget.Label"},
    priority = -1000,
    apply = {
        text = "", -- Default empty text
        foreground = "#ffffff",
        opacity = 1.0,
    }
}

function Label.new(options)
    local self = setmetatable({}, Label)
    -- options.getText is an optional callback function () -> string
    if options and options.getText then
        self.getText = options.getText
    end
    return self
end

function Label:draw(id, x, y, w, h, parentLWC)
    -- Combine generic type tag with specific instance ID tag
    local lwc = setbox.newContext({"widget.Label", "id." .. id}, parentLWC)
    
    local text = ""
    if self.getText then
        text = self.getText(lwc)
    elseif lwc:has("text") then
        text = lwc:getString("text")
    else
        -- Fallback to title property if no text property
        text = lwc:has("title") and lwc:getString("title") or ""
    end
    
    TextMixin.draw(x, y, text, lwc)
    
    -- Return size for layout systems
    return TextMixin.measure(text), TextMixin.getLineHeight()
end

return Label
