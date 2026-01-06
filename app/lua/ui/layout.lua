--[[
  UI Layout System

  Provides automatic positioning and sizing for widgets using:
  - Horizontal/Vertical stacking layouts
  - Dock regions (top, bottom, left, right, center)
  - Split panels

  Usage:
    layout.begin(0, 0, windowW, windowH)

    layout.dock("top", 40)
    -- draw top bar widgets here
    layout.endDock()

    layout.dock("left", 200)
    -- draw sidebar widgets here
    layout.endDock()

    -- remaining area is center
    local cx, cy, cw, ch = layout.getRect()
    -- draw main content

    layout.finish()
]]

local layout = {}

-- Layout region stack
local regionStack = {}
local currentRegion = nil

-- Spacing and padding defaults
layout.defaultPadding = 8
layout.defaultSpacing = 4

-- Region structure
local function createRegion(x, y, w, h)
    return {
        x = x,
        y = y,
        w = w,
        h = h,
        -- Cursor for sequential layout
        cursorX = x,
        cursorY = y,
        -- Layout mode: "free", "horizontal", "vertical"
        mode = "free",
        -- Spacing between items
        spacing = layout.defaultSpacing,
        -- Padding inside region
        padding = layout.defaultPadding,
        -- Track max dimensions for auto-sizing
        maxW = 0,
        maxH = 0,
        -- Items placed in this region
        itemCount = 0,
    }
end

-- Begin layout for a frame
function layout.begin(x, y, w, h)
    regionStack = {}
    currentRegion = createRegion(x, y, w, h)
    table.insert(regionStack, currentRegion)
end

-- Finish layout for a frame
function layout.finish()
    regionStack = {}
    currentRegion = nil
end

-- Get current region rect
function layout.getRect()
    if not currentRegion then return 0, 0, 0, 0 end
    return currentRegion.x, currentRegion.y, currentRegion.w, currentRegion.h
end

-- Get current cursor position
function layout.getCursor()
    if not currentRegion then return 0, 0 end
    return currentRegion.cursorX, currentRegion.cursorY
end

-- Get remaining width/height in current region
function layout.getRemainingSize()
    if not currentRegion then return 0, 0 end
    local r = currentRegion
    local remainingW = r.x + r.w - r.cursorX
    local remainingH = r.y + r.h - r.cursorY
    return remainingW, remainingH
end

-- Push a new sub-region
local function pushRegion(x, y, w, h)
    local region = createRegion(x, y, w, h)
    table.insert(regionStack, region)
    currentRegion = region
    return region
end

-- Pop back to parent region
local function popRegion()
    if #regionStack > 1 then
        table.remove(regionStack)
        currentRegion = regionStack[#regionStack]
    end
    return currentRegion
end

-- Dock a region to a side
-- side: "top", "bottom", "left", "right"
-- size: height for top/bottom, width for left/right
function layout.dock(side, size)
    if not currentRegion then return end
    local r = currentRegion
    local x, y, w, h

    if side == "top" then
        x, y, w, h = r.x, r.y, r.w, size
        r.y = r.y + size
        r.h = r.h - size
        r.cursorY = r.y
    elseif side == "bottom" then
        x, y, w, h = r.x, r.y + r.h - size, r.w, size
        r.h = r.h - size
    elseif side == "left" then
        x, y, w, h = r.x, r.y, size, r.h
        r.x = r.x + size
        r.w = r.w - size
        r.cursorX = r.x
    elseif side == "right" then
        x, y, w, h = r.x + r.w - size, r.y, size, r.h
        r.w = r.w - size
    else
        return
    end

    pushRegion(x, y, w, h)
end

-- End a docked region
function layout.endDock()
    popRegion()
end

-- Split current region horizontally (left/right)
-- ratio: 0.0 to 1.0, portion for left side
-- Returns left region; call layout.nextSplit() for right
function layout.splitH(ratio)
    if not currentRegion then return end
    local r = currentRegion
    local leftW = math.floor(r.w * ratio)

    -- Store split info for nextSplit
    r.splitMode = "horizontal"
    r.splitRatio = ratio
    r.splitSize = leftW

    pushRegion(r.x, r.y, leftW, r.h)
end

-- Split current region vertically (top/bottom)
-- ratio: 0.0 to 1.0, portion for top side
function layout.splitV(ratio)
    if not currentRegion then return end
    local r = currentRegion
    local topH = math.floor(r.h * ratio)

    r.splitMode = "vertical"
    r.splitRatio = ratio
    r.splitSize = topH

    pushRegion(r.x, r.y, r.w, topH)
end

-- Move to the next split region
function layout.nextSplit()
    popRegion()
    if not currentRegion then return end
    local r = currentRegion

    if r.splitMode == "horizontal" then
        local rightX = r.x + r.splitSize
        local rightW = r.w - r.splitSize
        pushRegion(rightX, r.y, rightW, r.h)
    elseif r.splitMode == "vertical" then
        local bottomY = r.y + r.splitSize
        local bottomH = r.h - r.splitSize
        pushRegion(r.x, bottomY, r.w, bottomH)
    end
end

-- End a split region
function layout.endSplit()
    popRegion()
    if currentRegion then
        currentRegion.splitMode = nil
        currentRegion.splitSize = nil
    end
end

-- Begin a horizontal layout (items placed left to right)
function layout.beginHorizontal(spacing)
    if not currentRegion then return end
    local r = currentRegion
    local region = pushRegion(r.cursorX, r.cursorY, r.w - (r.cursorX - r.x), r.h - (r.cursorY - r.y))
    region.mode = "horizontal"
    region.spacing = spacing or layout.defaultSpacing
    region.cursorX = region.x
    region.cursorY = region.y
end

-- End horizontal layout
function layout.endHorizontal()
    local r = currentRegion
    local itemH = r.maxH
    popRegion()
    if currentRegion and itemH > 0 then
        currentRegion.cursorY = currentRegion.cursorY + itemH + currentRegion.spacing
    end
end

-- Begin a vertical layout (items placed top to bottom)
function layout.beginVertical(spacing)
    if not currentRegion then return end
    local r = currentRegion
    local region = pushRegion(r.cursorX, r.cursorY, r.w - (r.cursorX - r.x), r.h - (r.cursorY - r.y))
    region.mode = "vertical"
    region.spacing = spacing or layout.defaultSpacing
    region.cursorX = region.x
    region.cursorY = region.y
end

-- End vertical layout
function layout.endVertical()
    local r = currentRegion
    local itemW = r.maxW
    popRegion()
    if currentRegion and itemW > 0 then
        currentRegion.cursorX = currentRegion.cursorX + itemW + currentRegion.spacing
    end
end

-- Reserve space for a widget and return its position
-- Returns x, y and advances cursor based on layout mode
function layout.reserveSpace(w, h)
    if not currentRegion then return 0, 0 end
    local r = currentRegion

    local x, y = r.cursorX, r.cursorY

    if r.mode == "horizontal" then
        r.cursorX = r.cursorX + w + r.spacing
        r.maxH = math.max(r.maxH, h)
        r.maxW = r.maxW + w + r.spacing
    elseif r.mode == "vertical" then
        r.cursorY = r.cursorY + h + r.spacing
        r.maxW = math.max(r.maxW, w)
        r.maxH = r.maxH + h + r.spacing
    else
        -- Free mode - don't advance cursor
    end

    r.itemCount = r.itemCount + 1
    return x, y
end

-- Add spacing
function layout.space(amount)
    if not currentRegion then return end
    local r = currentRegion

    if r.mode == "horizontal" then
        r.cursorX = r.cursorX + amount
    elseif r.mode == "vertical" then
        r.cursorY = r.cursorY + amount
    end
end

-- Set cursor position within current region
function layout.setCursor(x, y)
    if not currentRegion then return end
    currentRegion.cursorX = currentRegion.x + x
    currentRegion.cursorY = currentRegion.y + y
end

-- Move cursor to next row (for free/vertical mode)
function layout.newLine(height)
    if not currentRegion then return end
    local r = currentRegion
    height = height or r.maxH
    if height == 0 then height = 20 end  -- Default line height
    r.cursorX = r.x
    r.cursorY = r.cursorY + height + r.spacing
    r.maxH = 0
end

-- Indent cursor
function layout.indent(amount)
    if not currentRegion then return end
    amount = amount or layout.defaultPadding
    currentRegion.cursorX = currentRegion.cursorX + amount
end

-- Unindent cursor
function layout.unindent(amount)
    if not currentRegion then return end
    amount = amount or layout.defaultPadding
    currentRegion.cursorX = math.max(currentRegion.x, currentRegion.cursorX - amount)
end

-- Apply padding to current region (shrink it)
function layout.pad(padding)
    if not currentRegion then return end
    padding = padding or layout.defaultPadding
    local r = currentRegion
    r.x = r.x + padding
    r.y = r.y + padding
    r.w = r.w - padding * 2
    r.h = r.h - padding * 2
    r.cursorX = r.x
    r.cursorY = r.y
end

-- Center an item of given size within current region
function layout.center(w, h)
    if not currentRegion then return 0, 0 end
    local r = currentRegion
    local x = r.x + (r.w - w) / 2
    local y = r.y + (r.h - h) / 2
    return x, y
end

-- Align item to right edge of current region
function layout.alignRight(w)
    if not currentRegion then return 0 end
    return currentRegion.x + currentRegion.w - w
end

-- Align item to bottom edge of current region
function layout.alignBottom(h)
    if not currentRegion then return 0 end
    return currentRegion.y + currentRegion.h - h
end

-- Get current stack depth (for debugging)
function layout.getDepth()
    return #regionStack
end

return layout
