--[[
  UI Layout System
  Provides automatic positioning and sizing for widgets.
]]

local layout = {}

-- Layout region stack
local regionStack = {}
local currentRegion = nil
local nextRegionId = 1

-- Default rules for layout system (very low priority)
local setbox = require("SetBox")
setbox.rule {
    id = "layout-system-defaults",
    tags = {"layout.System"},
    priority = -1000,
    apply = {
        fallbackPadding = 8,
        fallbackSpacing = 4,
        fallbackLineHeight = 20,
    }
}

-- Defaults will be resolved from SetBox rules on first use
layout.defaultPadding = nil
layout.defaultSpacing = nil
layout.defaultLineHeight = nil

local function ensureDefaults()
    if not layout.defaultPadding or not layout.defaultSpacing or not layout.defaultLineHeight then
        local setbox = require("SetBox")
        -- Note: using pcall in case rules aren't loaded yet during early startup
        local ok, lwc = pcall(function() return setbox.newContext({"layout.System"}) end)
        if ok and lwc then
            layout.defaultPadding = lwc:getNumber("fallbackPadding")
            layout.defaultSpacing = lwc:getNumber("fallbackSpacing")
            layout.defaultLineHeight = lwc:getNumber("fallbackLineHeight")
        else
            -- Very basic fallbacks if SetBox is not ready
            layout.defaultPadding = 8
            layout.defaultSpacing = 4
            layout.defaultLineHeight = 20
        end
    end
end

local eventsModule = nil

local function createRegion(x, y, w, h, name)
    ensureDefaults()
    local id = "region_" .. nextRegionId
    nextRegionId = nextRegionId + 1
    return {
        id = id, name = name or id,
        x = x, y = y, w = w, h = h,
        cursorX = x, cursorY = y,
        mode = "free",
        spacing = layout.defaultSpacing,
        padding = layout.defaultPadding,
        maxW = 0, maxH = 0,
        itemCount = 0,
    }
end

function layout.setEventsModule(ev) eventsModule = ev end
function layout.getCurrentRegionId() return currentRegion and currentRegion.id or nil end
function layout.getCurrentRegionName() return currentRegion and currentRegion.name or nil end
function layout.getDepth() return #regionStack end

function layout.begin(x, y, w, h)
    regionStack = {}
    nextRegionId = 1
    currentRegion = createRegion(x, y, w, h, "root")
    table.insert(regionStack, currentRegion)
    if eventsModule and eventsModule.pushLayoutParent then eventsModule.pushLayoutParent(currentRegion.id) end
end

function layout.finish()
    if eventsModule and eventsModule.popLayoutParent then
        for i = 1, #regionStack do eventsModule.popLayoutParent() end
    end
    regionStack = {}
    currentRegion = nil
end

function layout.getRect() if not currentRegion then return 0,0,0,0 end return currentRegion.x, currentRegion.y, currentRegion.w, currentRegion.h end
function layout.getCursor() if not currentRegion then return 0,0 end return currentRegion.cursorX, currentRegion.cursorY end
function layout.getCursorX() if not currentRegion then return 0 end return currentRegion.cursorX end
function layout.getCursorY() if not currentRegion then return 0 end return currentRegion.cursorY end

function layout.getRemainingSize()
    if not currentRegion then return 0, 0 end
    local r = currentRegion
    return r.x + r.w - r.cursorX, r.y + r.h - r.cursorY
end

function layout.setRegion(x, y, w, h, name)
    local region = createRegion(x, y, w, h, name)
    table.insert(regionStack, region)
    currentRegion = region
    if eventsModule and eventsModule.pushLayoutParent then eventsModule.pushLayoutParent(region.id) end
end

function layout.endRegion()
    if eventsModule and eventsModule.popLayoutParent then eventsModule.popLayoutParent() end
    if #regionStack > 1 then
        table.remove(regionStack)
        currentRegion = regionStack[#regionStack]
    end
end

function layout.dock(side, size, name)
    if not currentRegion then return end
    local r = currentRegion
    local x, y, w, h
    if side == "top" then
        x, y, w, h = r.x, r.y, r.w, size
        r.y = r.y + size; r.h = r.h - size; r.cursorY = r.y
    elseif side == "bottom" then
        x, y, w, h = r.x, r.y + r.h - size, r.w, size
        r.h = r.h - size
    elseif side == "left" then
        x, y, w, h = r.x, r.y, size, r.h
        r.x = r.x + size; r.w = r.w - size; r.cursorX = r.x
    elseif side == "right" then
        x, y, w, h = r.x + r.w - size, r.y, size, r.h
        r.w = r.w - size
    else return end
    
    local region = createRegion(x, y, w, h, name or ("dock_" .. side))
    table.insert(regionStack, region)
    currentRegion = region
    if eventsModule and eventsModule.pushLayoutParent then eventsModule.pushLayoutParent(region.id) end
end

function layout.endDock() layout.endRegion() end

function layout.beginHorizontal(spacing, name)
    if not currentRegion then return end
    local r = currentRegion
    local region = createRegion(r.cursorX, r.cursorY, r.w - (r.cursorX - r.x), r.h - (r.cursorY - r.y), name or "hstack")
    region.mode = "horizontal"
    region.spacing = spacing or layout.defaultSpacing
    table.insert(regionStack, region)
    currentRegion = region
    if eventsModule and eventsModule.pushLayoutParent then eventsModule.pushLayoutParent(region.id) end
end

function layout.endHorizontal()
    local r = currentRegion
    local itemH = r.maxH
    layout.endRegion()
    if currentRegion and itemH > 0 then
        currentRegion.cursorY = currentRegion.cursorY + itemH + currentRegion.spacing
    end
end

function layout.reserveSpace(w, h)
    if not currentRegion then return 0, 0 end
    local r = currentRegion
    local x, y = r.cursorX, r.cursorY
    if r.mode == "horizontal" then
        r.cursorX = r.cursorX + w + r.spacing
        r.maxH = math.max(r.maxH, h)
    elseif r.mode == "vertical" then
        r.cursorY = r.cursorY + h + r.spacing
        r.maxW = math.max(r.maxW, w)
    end
    r.itemCount = r.itemCount + 1
    return x, y
end

function layout.space(amount)
    if not currentRegion then return end
    local r = currentRegion
    if r.mode == "horizontal" then r.cursorX = r.cursorX + amount
    elseif r.mode == "vertical" then r.cursorY = r.cursorY + amount end
end

function layout.newLine(height)
    if not currentRegion then return end
    ensureDefaults()
    local r = currentRegion
    height = height or r.maxH
    if height == 0 then height = layout.defaultLineHeight end
    r.cursorX = r.x
    r.cursorY = r.cursorY + height + r.spacing
    r.maxH = 0
end

function layout.pad(padding)
    if not currentRegion then return end
    padding = padding or layout.defaultPadding
    local r = currentRegion
    r.x = r.x + padding; r.y = r.y + padding
    r.w = r.w - padding * 2; r.h = r.h - padding * 2
    r.cursorX = r.x; r.cursorY = r.y
end

function layout.indent(amount)
    if not currentRegion then return end
    currentRegion.x = currentRegion.x + amount
    currentRegion.cursorX = currentRegion.cursorX + amount
end

function layout.unindent(amount)
    if not currentRegion then return end
    currentRegion.x = currentRegion.x - amount
    currentRegion.cursorX = currentRegion.cursorX - amount
end

function layout.center(w, h)
    if not currentRegion then return 0, 0 end
    local r = currentRegion
    return r.x + (r.w - w) / 2, r.y + (r.h - h) / 2
end

function layout.alignRight(w)
    if not currentRegion then return 0 end
    local r = currentRegion
    return r.x + r.w - w
end

function layout.alignBottom(h)
    if not currentRegion then return 0 end
    local r = currentRegion
    return r.y + r.h - h
end

function layout.splitH(ratio)
    if not currentRegion then return end
    local r = currentRegion
    local leftW = r.w * ratio
    layout.setRegion(r.x, r.y, leftW, r.h, "splitLeft")
end

function layout.splitV(ratio)
    if not currentRegion then return end
    local r = currentRegion
    local topH = r.h * ratio
    layout.setRegion(r.x, r.y, r.w, topH, "splitTop")
end

function layout.nextSplit()
    local oldRegion = currentRegion
    layout.endRegion()
    if not currentRegion then return end
    local r = currentRegion
    if oldRegion.name == "splitLeft" then
        layout.setRegion(r.x + oldRegion.w, r.y, r.w - oldRegion.w, r.h, "splitRight")
    elseif oldRegion.name == "splitTop" then
        layout.setRegion(r.x, r.y + oldRegion.h, r.w, r.h - oldRegion.h, "splitBottom")
    end
end

function layout.endSplit() layout.endRegion() end

function layout.beginVertical(spacing, name)
    if not currentRegion then return end
    local r = currentRegion
    local region = createRegion(r.cursorX, r.cursorY, r.w - (r.cursorX - r.x), r.h - (r.cursorY - r.y), name or "vstack")
    region.mode = "vertical"
    region.spacing = spacing or layout.defaultSpacing
    table.insert(regionStack, region)
    currentRegion = region
    if eventsModule and eventsModule.pushLayoutParent then eventsModule.pushLayoutParent(region.id) end
end

function layout.endVertical() layout.endRegion() end

return layout
