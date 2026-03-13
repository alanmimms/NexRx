--[[
    edit.lua - UI Editing Handlers (Disabled)

    Previously provided handlers for editing UI layout via modifier keys + mouse.
    Currently disabled to focus on automatic spring/magnet layout logic.
]]

local Edit = {}

-- Dependencies
local events = nil

-- =============================================================================
-- Initialization
-- =============================================================================

function Edit.init(eventsModule)
    events = eventsModule
end

-- =============================================================================
-- Handle Drawing (No-op)
-- =============================================================================

function Edit.drawHandles(mouseX, mouseY, widget, editModifierHeld)
    -- Disabled
end

function Edit.isEditModifierHeld(activeTags)
    return false
end

function Edit.getCurrentEdge()
    return nil
end

-- =============================================================================
-- Public API
-- =============================================================================

function Edit.isDragging()
    return false
end

function Edit.getDragState()
    return nil
end

return Edit
