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

-- Step fractions for generic slider adjustment
rule {
    id = "slider-step-default",
    tags = {"widget.Slider"},
    priority = 1,
    apply = { stepFraction = 0.01, factorMultiplier = 1.1 }
}

rule {
    id = "slider-step-ctrl",
    tags = {"widget.Slider", "input.CTRL"},
    priority = 2,
    apply = { stepFraction = 0.001, factorMultiplier = 1.01 }
}

rule {
    id = "slider-step-shift",
    tags = {"widget.Slider", "input.SHIFT"},
    priority = 2,
    apply = { stepFraction = 0.1, factorMultiplier = 1.5 }
}

rule {
    id = "slider-step-ctrl-shift",
    tags = {"widget.Slider", "input.CTRL", "input.SHIFT"},
    priority = 3,
    apply = { stepFraction = 0.25, factorMultiplier = 2.0 }
}

-- Generic slider handlers (matches any Slider widget with property in widget.data)
rule {
    id = "event-slider-wheel",
    tags = {"event.MouseWheel", "widget.Slider"},
    apply = { handler = "slider_adjust" }
}

rule {
    id = "event-slider-arrows",
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
rule {
    id = "event-slider-log-wheel",
    tags = {"event.MouseWheel", "widget.Slider", "widget.LogScale"},
    priority = 10,
    apply = { handler = "slider_adjust_log" }
}

-- =============================================================================
-- SignalBox Interaction
-- =============================================================================

-- Click to select
rule {
    id = "event-sb-click",
    tags = {"event.MouseDown-LEFT", "widget.SignalBox"},
    apply = { handler = "sb_drag_start" }
}

-- Dragging
rule {
    id = "event-sb-drag",
    tags = {"event.MouseMove", "widget.SignalBox", "state.Active"},
    apply = { handler = "sb_drag_move" }
}

rule {
    id = "event-sb-drag-end",
    tags = {"event.MouseUp-LEFT", "widget.SignalBox"},
    apply = { handler = "sb_drag_end" }
}

-- Navigation
rule {
    id = "event-sb-next",
    tags = {"event.KeyDown-TAB"},
    apply = { handler = "sb_next" }
}

rule {
    id = "event-sb-prev",
    tags = {"event.KeyDown-TAB", "input.SHIFT"},
    priority = 10,
    apply = { handler = "sb_prev" }
}

-- Naming
rule {
    id = "event-sb-name-start",
    tags = {"event.KeyDown-APOSTROPHE"},
    apply = { handler = "sb_name_start" }
}

rule {
    id = "event-sb-name-text",
    tags = {"event.TextInput", "state.SbNamingMode"},
    priority = 100,
    apply = { handler = "sb_name_text" }
}

rule {
    id = "event-sb-name-confirm",
    tags = {"event.KeyDown-ENTER", "state.SbNamingMode"},
    priority = 100,
    apply = { handler = "sb_name_confirm" }
}

rule {
    id = "event-sb-name-cancel",
    tags = {"event.KeyDown-ESC", "state.SbNamingMode"},
    priority = 100,
    apply = { handler = "sb_name_cancel" }
}

-- =============================================================================
-- VFO Control (Global tuning)
-- Frequency is in Hz.
-- =============================================================================

-- VFO Control Base Rules
rule {
    id = "event-vfo-wheel",
    tags = {"event.MouseWheel", "widget.VFOControl"},
    priority = 20,
    apply = { handler = "vfo_control" }
}

-- Global fallback for wheel (tunes VFO if nothing else handles it)
rule {
    id = "event-vfo-wheel-global",
    tags = {"event.MouseWheel"},
    priority = 1,
    apply = { handler = "vfo_control" }
}

-- Global VFO steps (apply when no specific widget tag is present)
rule {
    id = "vfo-step-global-default",
    tags = {"event.MouseWheel"},
    priority = 0,
    apply = { step = 100 }
}

rule {
    id = "vfo-step-global-ctrl",
    tags = {"event.MouseWheel", "input.CTRL"},
    priority = 1,
    apply = { step = 10000 }
}

rule {
    id = "vfo-step-global-shift",
    tags = {"event.MouseWheel", "input.SHIFT"},
    priority = 1,
    apply = { step = 100000 }
}

rule {
    id = "vfo-step-global-ctrl-shift",
    tags = {"event.MouseWheel", "input.CTRL", "input.SHIFT"},
    priority = 2,
    apply = { step = 1000 }
}

-- Default steps for VFO
rule {
    id = "vfo-step-default",
    tags = {"widget.VFOControl"},
    priority = 1,
    apply = { step = 100 }
}

rule {
    id = "vfo-step-ctrl",
    tags = {"widget.VFOControl", "input.CTRL"},
    priority = 2,
    apply = { step = 10000 }
}

rule {
    id = "vfo-step-shift",
    tags = {"widget.VFOControl", "input.SHIFT"},
    priority = 2,
    apply = { step = 100000 }
}

rule {
    id = "vfo-step-ctrl-shift",
    tags = {"widget.VFOControl", "input.CTRL", "input.SHIFT"},
    priority = 3,
    apply = { step = 1000 }
}

rule {
    id = "vfo-step-h",
    tags = {"widget.VFOControl", "input.H"},
    priority = 2,
    apply = { step = 10 }
}

rule {
    id = "vfo-step-shift-h",
    tags = {"widget.VFOControl", "input.SHIFT", "input.H"},
    priority = 3,
    apply = { step = 100 }
}

-- VFO Highlight when hovered anywhere within VFO control area (VFO, Spectrum, WF)
rule {
    id = "vfo-highlight-vfo",
    tags = {"widget.FrequencyDisplay", "widget.VFOControl"},
    priority = 10,
    apply = { highlighted = true }
}

-- Digit keys while hovering VFO start entry
for i=0,9 do
rule {
    id = "event-vfo-digit-" .. i,
    tags = {"event.KeyDown-" .. i, "widget.VFOControl"},
    priority = 50,
    apply = { handler = "freq_entry_start" }
}
end

-- VFO Highlight when in entry mode
rule {
    id = "vfo-highlight-entry",
    tags = {"widget.FrequencyDisplay", "state.VFOEditing"},
    priority = 20,
    apply = { highlighted = true }
}

-- =============================================================================
-- ISG Control (Internal Signal Gen)
-- =============================================================================

-- Base ISG rule
rule {
    id = "event-isg-wheel",
    tags = {"event.MouseWheel", "widget.IsgControl"},
    priority = 100,
    apply = { handler = "vfo_control", property = "isgFrequency" }
}

-- Default steps for ISG (same as VFO)
rule {
    id = "isg-step-default",
    tags = {"widget.IsgControl"},
    priority = 1,
    apply = { step = 100 }
}

rule {
    id = "isg-step-ctrl",
    tags = {"widget.IsgControl", "input.CTRL"},
    priority = 2,
    apply = { step = 10000 }
}

rule {
    id = "isg-step-shift",
    tags = {"widget.IsgControl", "input.SHIFT"},
    priority = 2,
    apply = { step = 100000 }
}

rule {
    id = "isg-step-ctrl-shift",
    tags = {"widget.IsgControl", "input.CTRL", "input.SHIFT"},
    priority = 3,
    apply = { step = 1000 }
}

rule {
    id = "isg-step-h",
    tags = {"widget.IsgControl", "input.H"},
    priority = 2,
    apply = { step = 10 }
}

rule {
    id = "isg-step-shift-h",
    tags = {"widget.IsgControl", "input.SHIFT", "input.H"},
    priority = 3,
    apply = { step = 100 }
}

-- ISG Arrow keys
rule {
    id = "event-isg-arrow-right",
    tags = {"event.KeyDown-RIGHT", "widget.IsgControl"},
    priority = 100,
    apply = { handler = "vfo_control", property = "isgFrequency" }
}

rule {
    id = "event-isg-arrow-left",
    tags = {"event.KeyDown-LEFT", "widget.IsgControl"},
    priority = 100,
    apply = { handler = "vfo_control", property = "isgFrequency" }
}

-- Digit keys while hovering ISG start entry
for i=0,9 do
rule {
    id = "event-isg-digit-" .. i,
    tags = {"event.KeyDown-" .. i, "widget.IsgControl"},
    priority = 150,
    apply = { handler = "freq_entry_start" }
}
end

-- ISG Highlight
rule {
    id = "isg-highlight-hover",
    tags = {"widget.FrequencyDisplay", "widget.IsgControl"},
    priority = 10,
    apply = { highlighted = true }
}

rule {
    id = "isg-highlight-entry",
    tags = {"widget.FrequencyDisplay", "state.ISGEditing"},
    priority = 20,
    apply = { highlighted = true }
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
    id = "event-freq-entry-move-left",
    tags = {"event.KeyDown-LEFT", "state.FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_move" }
}

rule {
    id = "event-freq-entry-move-right",
    tags = {"event.KeyDown-RIGHT", "state.FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_move" }
}

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
