--[[
    NexRx Event Handler Rules (Unified Tag Architecture)

    Defines SetBox rules that map event + widget + modifier combinations
    to handler function names.

    Tag namespaces:
    - event.*   : Transient event tags (MouseDown-LEFT, KeyDown-H, etc.)
    - widget.*  : Widget type and identity (Button, Slider, VFOControl, etc.)
    - state.*   : Widget/app state (Hovered, Active, Mode-USB, etc.)
    - input.*   : Held inputs (SHIFT, CTRL, ALT, MouseLEFT, etc.)

    Event tag format:
    - event.MouseDown-LEFT, event.MouseDown-MIDDLE, event.MouseDown-RIGHT
    - event.MouseUp-LEFT, event.MouseUp-MIDDLE, event.MouseUp-RIGHT
    - event.MouseWheel
    - event.KeyDown-H, event.KeyDown-ESC, event.KeyDown-ENTER
    - event.KeyUp-H
    - event.TextInput

    Key/button names are UPPERCASE: LEFT, MIDDLE, RIGHT, H, ESC, ENTER, Q, F
    Modifiers are UPPERCASE: SHIFT, CTRL, ALT, LSHIFT, RSHIFT

    Scoring: Sum of tag priorities (default @1), higher wins.
    Use @N suffix for explicit priority: "widget.VFOControl@20"
]]

-- =============================================================================
-- Generic Slider Controls
-- Handler gets min/max/property from widget.data, calculates steps as % of range
-- Steps: default=1%, ctrl=0.1%, shift=10%, ctrl+shift=25%
-- =============================================================================

-- Slider click to activate (start drag)
rule {
    id = "event-slider-click",
    tags = {"event.MouseDown-LEFT", "widget.Slider"},
    apply = { handler = "slider_activate" }
}

-- Generic slider wheel (matches any Slider widget with property in widget.data)
rule {
    id = "event-slider-wheel",
    tags = {"event.MouseWheel", "widget.Slider"},
    apply = { handler = "slider_adjust" }
}

-- Generic slider arrow keys
rule {
    id = "event-slider-arrow-right",
    tags = {"event.KeyDown-RIGHT", "widget.Slider"},
    apply = { handler = "slider_adjust" }
}

rule {
    id = "event-slider-arrow-left",
    tags = {"event.KeyDown-LEFT", "widget.Slider"},
    apply = { handler = "slider_adjust" }
}

rule {
    id = "event-slider-arrow-up",
    tags = {"event.KeyDown-UP", "widget.Slider"},
    apply = { handler = "slider_adjust" }
}

rule {
    id = "event-slider-arrow-down",
    tags = {"event.KeyDown-DOWN", "widget.Slider"},
    apply = { handler = "slider_adjust" }
}

-- Logarithmic slider (for LogScale tagged sliders like LMS mu)
-- Uses multiplicative factors: default=1.5x, ctrl=1.1x, shift=2x, ctrl+shift=5x
rule {
    id = "event-slider-log-wheel",
    tags = {"event.MouseWheel", "widget.Slider", "widget.LogScale"},
    priority = 10,
    apply = { handler = "slider_adjust_log" }
}

-- =============================================================================
-- VFO Control (Global tuning)
-- Frequency is in Hz.
-- =============================================================================

-- VFO Wheel: 100 Hz steps (no modifiers)
rule {
    id = "event-vfo-wheel",
    tags = {"event.MouseWheel"},
    priority = 20,
    apply = { handler = "vfo_control", step = 100 }
}

-- VFO + CTRL: 10 kHz steps
rule {
    id = "event-vfo-wheel-ctrl",
    tags = {"event.MouseWheel", "input.CTRL"},
    priority = 30,
    apply = { handler = "vfo_control", step = 10000 }
}

-- VFO + SHIFT: 100 kHz steps
rule {
    id = "event-vfo-wheel-shift",
    tags = {"event.MouseWheel", "input.SHIFT"},
    priority = 30,
    apply = { handler = "vfo_control", step = 100000 }
}

-- VFO + CTRL + SHIFT: 1 kHz steps
rule {
    id = "event-vfo-wheel-ctrl-shift",
    tags = {"event.MouseWheel", "input.CTRL", "input.SHIFT"},
    priority = 40,
    apply = { handler = "vfo_control", step = 1000 }
}

-- VFO + H: 10 Hz fine tuning
rule {
    id = "event-vfo-wheel-h",
    tags = {"event.MouseWheel", "input.H"},
    priority = 30,
    apply = { handler = "vfo_control", step = 10 }
}

-- VFO + SHIFT + H: 100 Hz fine tuning
rule {
    id = "event-vfo-wheel-shift-h",
    tags = {"event.MouseWheel", "input.SHIFT", "input.H"},
    priority = 40,
    apply = { handler = "vfo_control", step = 100 }
}

-- =============================================================================
-- ISG Control (Internal Signal Gen)
-- =============================================================================

rule {
    id = "event-isg-wheel",
    tags = {"event.MouseWheel", "widget.IsgControl"},
    priority = 100,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 100 }
}

rule {
    id = "event-isg-wheel-ctrl",
    tags = {"event.MouseWheel", "widget.IsgControl", "input.CTRL"},
    priority = 110,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 10000 }
}

rule {
    id = "event-isg-wheel-shift",
    tags = {"event.MouseWheel", "widget.IsgControl", "input.SHIFT"},
    priority = 110,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 100000 }
}

rule {
    id = "event-isg-wheel-ctrl-shift",
    tags = {"event.MouseWheel", "widget.IsgControl", "input.CTRL", "input.SHIFT"},
    priority = 120,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 1000 }
}

rule {
    id = "event-isg-wheel-h",
    tags = {"event.MouseWheel", "widget.IsgControl", "input.H"},
    priority = 110,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 10 }
}

rule {
    id = "event-isg-wheel-shift-h",
    tags = {"event.MouseWheel", "widget.IsgControl", "input.SHIFT", "input.H"},
    priority = 120,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 100 }
}

rule {
    id = "event-isg-arrow-right",
    tags = {"event.KeyDown-RIGHT", "widget.IsgControl"},
    priority = 100,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 0.001 }
}

rule {
    id = "event-isg-arrow-left",
    tags = {"event.KeyDown-LEFT", "widget.IsgControl"},
    priority = 100,
    apply = { handler = "vfo_control", property = "isgFrequency", step = -0.001 }
}

-- KeyUp on VFOControl widgets - silently consume (keys used as modifiers)
rule {
    id = "event-vfo-keyup-h",
    tags = {"event.KeyUp-H", "widget.VFOControl"},
    apply = { handler = "noop" }
}

-- =============================================================================
-- Frequency Entry Mode
-- =============================================================================

rule {
    id = "event-freq-entry-start-f",
    tags = {"event.KeyDown-F"},
    apply = { handler = "freq_entry_start" }
}

rule {
    id = "event-freq-entry-start-click",
    tags = {"event.MouseDown-LEFT", "widget.FrequencyDisplay"},
    apply = { handler = "freq_entry_start" }
}

rule {
    id = "event-freq-entry-cancel",
    tags = {"event.KeyDown-ESC", "state.FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_cancel" }
}

rule {
    id = "event-freq-entry-confirm",
    tags = {"event.KeyDown-ENTER", "state.FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_confirm" }
}

rule {
    id = "event-freq-entry-backspace",
    tags = {"event.KeyDown-BACKSPACE", "state.FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_backspace" }
}

rule {
    id = "event-freq-entry-text",
    tags = {"event.TextInput", "state.FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_text" }
}

-- =============================================================================
-- Application Control (Global)
-- =============================================================================

rule {
    id = "event-app-quit",
    tags = {"event.KeyDown-Q", "input.CTRL"},
    priority = 1000,
    apply = { handler = "app_quit" }
}

rule {
    id = "event-debug-toggle",
    tags = {"event.KeyDown-D", "input.CTRL"},
    priority = 1000,
    apply = { handler = "debug_toggle" }
}

rule {
    id = "event-layout-reset",
    tags = {"event.KeyDown-R", "input.CTRL", "input.ALT"},
    priority = 1000,
    apply = { handler = "layout_reset" }
}

-- =============================================================================
-- Fallback: Unhandled Events
-- =============================================================================

rule {
    id = "event-unhandled",
    tags = {"event.Unhandled"},
    priority = -1000,
    apply = { handler = "log_unhandled" }
}

print("[events.lua] Event handler rules loaded (with Design Mode)")
