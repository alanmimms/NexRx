--[[
    edit.lua - UI Editing Handlers

    Provides handlers for editing UI layout via modifier keys + mouse:
    - Ctrl+Alt+Left drag on edge = resize
    - Ctrl+Alt+Middle drag = move
    - Ctrl+Alt+Right click = context menu (future)

    When edit modifier is held:
    - Resize handles appear on hovered widget edges
    - Handles indicate which edges can be dragged

    No "mode" - editing is always available via modifier keys.
    Rules in config/events.lua determine when these handlers fire.

    Usage:
        local Edit = require("edit")
        Edit.init(events, AppState)

        -- In draw(), after widgets but before endFrame:
        Edit.drawHandles(mouseX, mouseY, hoveredWidget, isEditModifierHeld)
]]

local Edit = {}

-- Dependencies
local events = nil
local layoutOverrides = require("layout_overrides")
local constraints = require("ui.constraints")

-- Drag state (only while actively dragging)
local dragState = nil

-- Edge detection threshold (pixels from edge to count as edge)
local EDGE_THRESHOLD = 12

-- Handle visual settings
local HANDLE_SIZE = 8
local HANDLE_COLOR = {0.3, 0.7, 1.0, 0.8}        -- Blue
local HANDLE_HOVER_COLOR = {0.5, 0.9, 1.0, 1.0}  -- Bright cyan when on edge
local HANDLE_DRAG_COLOR = {1.0, 0.8, 0.3, 1.0}   -- Orange when dragging

-- Current detected edge (for visual feedback)
local currentEdge = nil

-- =============================================================================
-- Initialization
-- =============================================================================

function Edit.init(eventsModule)
    events = eventsModule

    -- Load any existing layout overrides
    layoutOverrides.load()

    Edit._registerHandlers()
    print("[Edit] Handlers registered (Ctrl+Alt+drag to resize/move)")
end

function Edit._registerHandlers()
    -- Resize start (Ctrl+Alt+Left on widget edge)
    events.registerHandler("edit_resize_start", function(event, widget)
        if not widget then return false end

        local edge = Edit._detectEdge(event.x, event.y, widget)
        if not edge then return false end

        local properties = Edit._getWidgetProperties(widget.id, edge)
        if not properties then return false end

        local b = widget.bounds
        local winW, winH = getWindowSize()

        -- Get current layout sizes from override or actual bounds
        local currentW = layoutOverrides.get(widget.id, "width") or b.w
        local currentH = layoutOverrides.get(widget.id, "height") or b.h

        print(string.format("[Edit] dragStart: bounds=(%d,%d,%d,%d) win=%dx%d currentW=%d",
            b.x, b.y, b.w, b.h, winW, winH, currentW))

        dragState = {
            startWindowW = winW,
            startWindowH = winH,
            initialWidth = currentW,
            initialHeight = currentH,
            action = "resize",
            edge = edge,
            widgetId = widget.id,
            startX = event.x,
            startY = event.y,
            startBounds = { x = b.x, y = b.y, w = b.w, h = b.h },
            currentWidth = currentW,
            currentHeight = currentH,
            properties = properties,
            atRight = b.x + b.w >= winW - 20,
            atBottom = b.y + b.h >= winH - 20,
            atLeft = b.x <= 20,
            atTop = b.y <= 20,
        }

        print(string.format("[Edit] Resize start: %s edge=%s", widget.id, edge))
        return true
    end)

    -- Move start (Ctrl+Alt+Middle anywhere on widget)
    events.registerHandler("edit_move_start", function(event, widget)
        if not widget then return false end

        dragState = {
            action = "move",
            widgetId = widget.id,
            startX = event.x,
            startY = event.y,
            startBounds = {
                x = widget.bounds.x,
                y = widget.bounds.y,
                w = widget.bounds.w,
                h = widget.bounds.h,
            },
        }

        print(string.format("[Edit] Move start: %s", widget.id))
        return true
    end)

    -- Drag continue (handles both resize and move)
    events.registerHandler("edit_drag", function(event, widget)
        if not dragState then return false end

        -- Store current mouse position for _applyResize
        dragState.currentMouseX = event.x
        dragState.currentMouseY = event.y

        local dx = event.x - dragState.startX
        local dy = event.y - dragState.startY

        if dragState.action == "resize" then
            Edit._applyResize(dx, dy)
        elseif dragState.action == "move" then
            Edit._applyMove(dx, dy)
        end

        return true
    end)

    -- Drag end
    events.registerHandler("edit_drag_end", function(event, widget)
        if not dragState then return false end

        local winW, winH = getWindowSize()
        local overrideW = layoutOverrides.get(dragState.widgetId, "width")
        print(string.format("[Edit] %s end: %s, override=%s, window=%dx%d",
            dragState.action, dragState.widgetId,
            overrideW and tostring(overrideW) or "nil",
            winW, winH))

        Edit._serialize()
        dragState = nil
        return true
    end)

    -- Context menu (Ctrl+Alt+Right) - placeholder for future
    events.registerHandler("edit_context_menu", function(event, widget)
        if not widget then return false end
        print(string.format("[Edit] Context menu: %s (not yet implemented)", widget.id))
        return true
    end)
end

-- =============================================================================
-- Edge Detection
-- =============================================================================

function Edit._detectEdge(mx, my, widget)
    local b = widget.bounds
    if not b then return nil end

    local onLeft = mx >= b.x and mx <= b.x + EDGE_THRESHOLD
    local onRight = mx >= b.x + b.w - EDGE_THRESHOLD and mx <= b.x + b.w
    local onTop = my >= b.y and my <= b.y + EDGE_THRESHOLD
    local onBottom = my >= b.y + b.h - EDGE_THRESHOLD and my <= b.y + b.h

    -- Corners
    if onTop and onLeft then return "top-left" end
    if onTop and onRight then return "top-right" end
    if onBottom and onLeft then return "bottom-left" end
    if onBottom and onRight then return "bottom-right" end

    -- Edges
    if onLeft then return "left" end
    if onRight then return "right" end
    if onTop then return "top" end
    if onBottom then return "bottom" end

    return nil  -- Not on edge (interior)
end

-- =============================================================================
-- Widget to Property Mapping
-- =============================================================================

function Edit._getWidgetProperties(widgetId, edge)
    -- Map edge/corner to size properties
    -- Returns { width = bool, height = bool, widthSign = 1/-1, heightSign = 1/-1 }
    -- Sign indicates whether positive drag increases or decreases the dimension
    -- All edges are always resizable

    if edge == "left" then
        return { width = true, height = false, widthSign = -1, heightSign = 1 }
    elseif edge == "right" then
        return { width = true, height = false, widthSign = 1, heightSign = 1 }
    elseif edge == "top" then
        return { width = false, height = true, widthSign = 1, heightSign = -1 }
    elseif edge == "bottom" then
        return { width = false, height = true, widthSign = 1, heightSign = 1 }
    elseif edge == "top-left" then
        return { width = true, height = true, widthSign = -1, heightSign = -1 }
    elseif edge == "top-right" then
        return { width = true, height = true, widthSign = 1, heightSign = -1 }
    elseif edge == "bottom-left" then
        return { width = true, height = true, widthSign = -1, heightSign = 1 }
    elseif edge == "bottom-right" then
        return { width = true, height = true, widthSign = 1, heightSign = 1 }
    end

    return nil
end

-- =============================================================================
-- Apply Resize
-- =============================================================================

function Edit._applyResize(dx, dy)
    if not dragState or dragState.action ~= "resize" then return end

    local props = dragState.properties
    local edge = dragState.edge
    local winW, winH = getWindowSize()
    local widgetId = dragState.widgetId

    if props.width then
        -- Calculate total mouse movement from drag start
        local totalDx = dragState.currentMouseX - dragState.startX

        if (edge == "right" or edge == "top-right" or edge == "bottom-right") and dragState.atRight then
            -- At right edge: resize both window and widget
            dragState.currentWidth = dragState.initialWidth + totalDx
            setWindowSize(dragState.startWindowW + totalDx, winH)
        elseif (edge == "left" or edge == "top-left" or edge == "bottom-left") and dragState.atLeft then
            dragState.currentWidth = dragState.initialWidth - totalDx
            setWindowSize(dragState.startWindowW - totalDx, winH)
        else
            -- Not at window edge: resize widget using total delta from start
            dragState.currentWidth = dragState.initialWidth + totalDx * props.widthSign
        end
        layoutOverrides.set(widgetId, "width", dragState.currentWidth)
    end

    if props.height then
        local totalDy = dragState.currentMouseY - dragState.startY

        if (edge == "bottom" or edge == "bottom-left" or edge == "bottom-right") and dragState.atBottom then
            dragState.currentHeight = dragState.initialHeight + totalDy
            local curWinW = select(1, getWindowSize())
            setWindowSize(curWinW, dragState.startWindowH + totalDy)
        elseif (edge == "top" or edge == "top-left" or edge == "top-right") and dragState.atTop then
            dragState.currentHeight = dragState.initialHeight - totalDy
            local curWinW = select(1, getWindowSize())
            setWindowSize(curWinW, dragState.startWindowH - totalDy)
        else
            -- Not at window edge: resize widget using total delta from start
            dragState.currentHeight = dragState.initialHeight + totalDy * props.heightSign
        end
        layoutOverrides.set(widgetId, "height", dragState.currentHeight)
    end
end

-- =============================================================================
-- Apply Move
-- =============================================================================

function Edit._applyMove(dx, dy)
    if not dragState or dragState.action ~= "move" then return end

    -- For now, just log - moving arbitrary widgets requires more infrastructure
    -- (storing widget positions as reactive properties)
    print(string.format("[Edit] Move delta: dx=%d dy=%d (not yet persisted)", dx, dy))
end

-- =============================================================================
-- Serialization
-- =============================================================================

function Edit._serialize()
    layoutOverrides.save()
end

-- =============================================================================
-- Handle Drawing
-- =============================================================================

--- Draw resize handles on a widget when edit modifier is held
-- @param mouseX, mouseY Current mouse position
-- @param widget The hovered widget (or nil)
-- @param editModifierHeld True if Ctrl+Alt (or future Z key) is held
function Edit.drawHandles(mouseX, mouseY, widget, editModifierHeld)
    if not editModifierHeld then
        currentEdge = nil
        return
    end

    if not widget or not widget.bounds then
        currentEdge = nil
        return
    end

    -- Detect which edge mouse is near
    currentEdge = Edit._detectEdge(mouseX, mouseY, widget)

    local b = widget.bounds
    local hs = HANDLE_SIZE

    -- Draw corner handles
    local corners = {
        { x = b.x, y = b.y, edge = "top-left" },
        { x = b.x + b.w - hs, y = b.y, edge = "top-right" },
        { x = b.x, y = b.y + b.h - hs, edge = "bottom-left" },
        { x = b.x + b.w - hs, y = b.y + b.h - hs, edge = "bottom-right" },
    }

    for _, corner in ipairs(corners) do
        local color = HANDLE_COLOR
        if dragState and dragState.edge == corner.edge then
            color = HANDLE_DRAG_COLOR
        elseif currentEdge == corner.edge then
            color = HANDLE_HOVER_COLOR
        end
        drawRect(corner.x, corner.y, hs, hs, color[1], color[2], color[3], color[4])
    end

    -- Draw edge handles (centered on each edge)
    local edges = {
        { x = b.x + b.w/2 - hs/2, y = b.y, w = hs, h = hs, edge = "top" },
        { x = b.x + b.w/2 - hs/2, y = b.y + b.h - hs, w = hs, h = hs, edge = "bottom" },
        { x = b.x, y = b.y + b.h/2 - hs/2, w = hs, h = hs, edge = "left" },
        { x = b.x + b.w - hs, y = b.y + b.h/2 - hs/2, w = hs, h = hs, edge = "right" },
    }

    for _, e in ipairs(edges) do
        local color = HANDLE_COLOR
        if dragState and dragState.edge == e.edge then
            color = HANDLE_DRAG_COLOR
        elseif currentEdge == e.edge then
            color = HANDLE_HOVER_COLOR
        end
        drawRect(e.x, e.y, e.w, e.h, color[1], color[2], color[3], color[4])
    end

    -- Draw outline around widget to indicate it's editable
    local outlineColor = dragState and HANDLE_DRAG_COLOR or HANDLE_COLOR
    drawRectOutline(b.x, b.y, b.w, b.h,
                    outlineColor[1], outlineColor[2], outlineColor[3], outlineColor[4], 2)
end

--- Check if edit modifier is currently held
-- Supports Ctrl+Alt now, can be extended to support Z key etc.
-- @param activeTags Table of currently active tags
function Edit.isEditModifierHeld(activeTags)
    -- Current: Ctrl+Alt
    -- Future: Could also check for input.Z or other modifiers
    local hasCtrl = activeTags["input.CTRL"] or activeTags["input.LCTRL"] or activeTags["input.RCTRL"]
    local hasAlt = activeTags["input.ALT"] or activeTags["input.LALT"] or activeTags["input.RALT"]

    return hasCtrl and hasAlt
end

--- Get the currently detected edge (for cursor hints etc.)
function Edit.getCurrentEdge()
    return currentEdge
end

-- =============================================================================
-- Public API
-- =============================================================================

function Edit.isDragging()
    return dragState ~= nil
end

function Edit.getDragState()
    return dragState
end

return Edit
