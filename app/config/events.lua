--[[
    NexRx Event Handler Rules

    Defines SetBox rules that map event + widget + modifier combinations
    to handler function names. The events.lua module resolves these rules
    to dispatch events appropriately.

    Tag structure:
    - "Event" - Always present for event rules
    - Event type: "MouseDown", "MouseUp", "MouseWheel", "KeyDown", etc.
    - Widget tags: "Button", "Slider", "FrequencyDisplay", etc.
    - Modifiers: "Shift", "Ctrl", "Alt"

    More specific rules (more tags) take precedence.
    Handler names reference functions registered via events.registerHandler().
]]

-- =============================================================================
-- Mouse Wheel Events
-- =============================================================================

-- Default mouse wheel (no specific widget) - does nothing
rule {
    id = "event-wheel-default",
    tags = {"Event", "MouseWheel"},
    priority = -100,
    apply = {
        handler = nil,  -- No handler = bubble up
    }
}

-- Mouse wheel on frequency display = tune (1 kHz steps)
rule {
    id = "event-freq-wheel",
    tags = {"Event", "MouseWheel", "FrequencyDisplay"},
    priority = 0,
    apply = {
        handler = "freq_tune",
        tuneStep = 0.001,  -- 1 kHz in MHz
    }
}

-- Shift+wheel on frequency display = coarse tune (100 kHz steps)
rule {
    id = "event-freq-wheel-shift",
    tags = {"Event", "MouseWheel", "FrequencyDisplay", "Shift"},
    priority = 10,
    apply = {
        handler = "freq_tune_coarse",
        tuneStep = 0.1,  -- 100 kHz in MHz
    }
}

-- Ctrl+wheel on frequency display = fine tune (100 Hz steps)
rule {
    id = "event-freq-wheel-ctrl",
    tags = {"Event", "MouseWheel", "FrequencyDisplay", "Ctrl"},
    priority = 10,
    apply = {
        handler = "freq_tune_fine",
        tuneStep = 0.0001,  -- 100 Hz in MHz
    }
}

-- Mouse wheel on slider = adjust value
rule {
    id = "event-slider-wheel",
    tags = {"Event", "MouseWheel", "Slider"},
    priority = 0,
    apply = {
        handler = "slider_wheel",
    }
}

-- Mouse wheel on waterfall = zoom/scroll (future)
rule {
    id = "event-waterfall-wheel",
    tags = {"Event", "MouseWheel", "Waterfall"},
    priority = 0,
    apply = {
        handler = "waterfall_wheel",
    }
}

-- =============================================================================
-- Mouse Button Events
-- =============================================================================

-- Mouse down on button = activate
rule {
    id = "event-button-down",
    tags = {"Event", "MouseDown", "Button"},
    priority = 0,
    apply = {
        handler = "button_press",
    }
}

-- Mouse up on button = release/click
rule {
    id = "event-button-up",
    tags = {"Event", "MouseUp", "Button"},
    priority = 0,
    apply = {
        handler = "button_release",
    }
}

-- Mouse down on slider = start drag
rule {
    id = "event-slider-down",
    tags = {"Event", "MouseDown", "Slider"},
    priority = 0,
    apply = {
        handler = "slider_start_drag",
    }
}

-- Mouse down on checkbox = toggle
rule {
    id = "event-checkbox-down",
    tags = {"Event", "MouseDown", "Checkbox"},
    priority = 0,
    apply = {
        handler = "checkbox_toggle",
    }
}

-- Mouse down on frequency display = start entry mode
rule {
    id = "event-freq-click",
    tags = {"Event", "MouseDown", "FrequencyDisplay"},
    priority = 0,
    apply = {
        handler = "freq_entry_start",
    }
}

-- Mouse down on waterfall = click-to-tune (future)
rule {
    id = "event-waterfall-click",
    tags = {"Event", "MouseDown", "Waterfall"},
    priority = 0,
    apply = {
        handler = "waterfall_click",
    }
}

-- =============================================================================
-- Keyboard Events
-- =============================================================================

-- Escape key (global) = cancel/quit
rule {
    id = "event-key-escape",
    tags = {"Event", "KeyDown", "Escape"},
    priority = 0,
    apply = {
        handler = "key_escape",
    }
}

-- F key = start frequency entry
rule {
    id = "event-key-f",
    tags = {"Event", "KeyDown", "F"},
    priority = 0,
    apply = {
        handler = "freq_entry_start",
    }
}

-- Enter key = confirm
rule {
    id = "event-key-enter",
    tags = {"Event", "KeyDown", "Enter"},
    priority = 0,
    apply = {
        handler = "key_enter",
    }
}

-- Backspace key = delete
rule {
    id = "event-key-backspace",
    tags = {"Event", "KeyDown", "Backspace"},
    priority = 0,
    apply = {
        handler = "key_backspace",
    }
}

-- =============================================================================
-- Text Input Events
-- =============================================================================

-- Text input while frequency entry active
rule {
    id = "event-text-freq-entry",
    tags = {"Event", "TextInput", "FrequencyEntry"},
    priority = 0,
    apply = {
        handler = "freq_entry_text",
    }
}

-- =============================================================================
-- Window Events
-- =============================================================================

-- Window resize
rule {
    id = "event-window-resize",
    tags = {"Event", "WindowResize"},
    priority = 0,
    apply = {
        handler = "window_resize",
    }
}

-- =============================================================================
-- Fallback: Unhandled Events
-- =============================================================================

-- Any unhandled event = log it
rule {
    id = "event-unhandled",
    tags = {"Event", "Unhandled"},
    priority = -1000,
    apply = {
        handler = "log_unhandled",
    }
}

-- Any event with no specific handler = let it bubble
rule {
    id = "event-default",
    tags = {"Event"},
    priority = -1000,
    apply = {
        handler = nil,  -- No handler = continue bubbling
    }
}

print("[events.lua] Event handler rules loaded")
