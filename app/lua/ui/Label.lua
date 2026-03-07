--[[
  Label Widget
  Modular UI component for text display.
  Everything comes from SetBox rules.
]]

local setbox = require("setbox")

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

function Label:draw(id, x, y, parentLWC)
    -- Combine generic type tag with specific instance ID tag
    local lwc = setbox.newContext({"widget.Label", "id." .. id}, parentLWC)
    
    local text = ""
    if self.getText then
        text = self.getText()
    else
        -- Static text MUST come from a rule matching this context
        text = lwc:getString("text")
    end
    
    -- All styling comes from rules
    local fgR, fgG, fgB = require("ui.widgets").hexToRgb(lwc:getString("foreground"))
    local alpha = lwc:getNumber("opacity")
    
    drawText(x, y, text, fgR, fgG, fgB, alpha)
    
    -- Return size for layout systems
    return measureText(text), getLineHeight()
end

return Label
