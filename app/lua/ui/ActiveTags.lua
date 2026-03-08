--[[
  Active Tags Debug Widget
  Modular UI component for displaying current SetBox tags.
  Behavior and style driven entirely by SetBox rules.
]]

local setbox = require("SetBox")
local Label = require("ui.Label")

local ActiveTags = {}
ActiveTags.__index = ActiveTags

-- Default rules for ActiveTags widget (very low priority)
setbox.rule {
    id = "active-tags-defaults",
    tags = {"widget.ActiveTagsViewer"},
    priority = -1000,
    apply = {
        background = "#08081a",
        opacity = 0.8,
        borderRadius = 4,
        padding = 8,
        title = "ACTIVE TAGS",
    }
}

-- Rule for the nested title label
setbox.rule {
    id = "active-tags-title",
    tags = {"widget.ActiveTagsViewer", "activeTags.title"},
    priority = -500,
    apply = {
        foreground = "#80b3ff",
        text = "ACTIVE TAGS",
    }
}

function ActiveTags.new()
    local self = setmetatable({}, ActiveTags)
    self.titleLabel = Label.new()
    self.tagLabel = Label.new()
    return self
end

function ActiveTags:draw(id, x, y, w, h, tags, parentLWC)
    local lwc = setbox.newContext({"widget.ActiveTagsViewer", "id." .. id}, parentLWC)
    
    -- Style resolution
    local bgR, bgG, bgB = require("ui.Widgets").hexToRgb(lwc:getString("background"))
    local radius = lwc:getNumber("borderRadius")
    local alpha = lwc:getNumber("opacity")
    local pad = lwc:getNumber("padding")
    
    drawRoundedRect(x, y, w, h, radius, bgR, bgG, bgB, alpha)
    
    -- Draw Title
    self.titleLabel:draw("active-tags-title", x + pad, y + pad, lwc)
    
    local ty = y + 32
    for _, tag in ipairs(tags) do
        -- Draw each tag using dynamic label text
        self.tagLabel.getText = function() return tag end
        self.tagLabel:draw(id .. "-tag-" .. tag, x + pad + 4, ty, lwc)
        ty = ty + 18
        if ty > y + h - 20 then break end
    end
end

return ActiveTags
