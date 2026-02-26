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
-- Frequency is in MHz, so: 1kHz=0.001, 10kHz=0.01, 100kHz=0.1, 1MHz=1.0
-- =============================================================================

-- VFO Wheel: 1 kHz steps (no modifiers)
rule {
    id = "event-vfo-wheel",
    tags = {"event.MouseWheel"},
    priority = 20,
    apply = { handler = "vfo_control", step = 0.001 }
}

-- VFO Arrows: 1 kHz steps
rule {
    id = "event-vfo-arrow-right",
    tags = {"event.KeyDown-RIGHT"},
    priority = 20,
    apply = { handler = "vfo_control", step = 0.001 }
}

rule {
    id = "event-vfo-arrow-left",
    tags = {"event.KeyDown-LEFT"},
    priority = 20,
    apply = { handler = "vfo_control", step = -0.001 }
}

rule {
    id = "event-vfo-arrow-up",
    tags = {"event.KeyDown-UP"},
    priority = 20,
    apply = { handler = "vfo_control", step = 0.001 }
}

rule {
    id = "event-vfo-arrow-down",
    tags = {"event.KeyDown-DOWN"},
    priority = 20,
    apply = { handler = "vfo_control", step = -0.001 }
}

-- VFO + CTRL: 10 kHz steps
rule {
    id = "event-vfo-wheel-ctrl",
    tags = {"event.MouseWheel", "input.CTRL"},
    priority = 30,
    apply = { handler = "vfo_control", step = 0.01 }
}

-- VFO + SHIFT: 1 MHz steps
rule {
    id = "event-vfo-wheel-shift",
    tags = {"event.MouseWheel", "input.SHIFT"},
    priority = 30,
    apply = { handler = "vfo_control", step = 1.0 }
}

-- VFO + CTRL + SHIFT: 100 kHz steps
rule {
    id = "event-vfo-wheel-ctrl-shift",
    tags = {"event.MouseWheel", "input.CTRL", "input.SHIFT"},
    priority = 40,
    apply = { handler = "vfo_control", step = 0.1 }
}

-- VFO + H: 10 Hz fine tuning
rule {
    id = "event-vfo-wheel-h",
    tags = {"event.MouseWheel", "input.H"},
    priority = 30,
    apply = { handler = "vfo_control", step = 0.00001 }
}

-- VFO + SHIFT + H: 100 Hz fine tuning
rule {
    id = "event-vfo-wheel-shift-h",
    tags = {"event.MouseWheel", "input.SHIFT", "input.H"},
    priority = 40,
    apply = { handler = "vfo_control", step = 0.0001 }
}

-- =============================================================================
-- ISG Control (Internal Signal Gen)
-- =============================================================================

rule {
    id = "event-isg-wheel",
    tags = {"event.MouseWheel", "widget.IsgControl"},
    priority = 100,
    apply = { handler = "vfo_control", property = "isgFrequency", step = 0.001 }
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

-- Generic checkbox toggle
rule {
    id = "event-checkbox-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Checkbox"},
    priority = 5,
    apply = { handler = "toggle_property" }
}

-- =============================================================================
-- VFO Button Clicks (specific logic, keep dedicated handlers)
-- =============================================================================

rule {
    id = "event-vfo-a-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.VfoA"},
    priority = 10,
    apply = { handler = "vfo_a_click" }
}

rule {
    id = "event-vfo-b-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.VfoB"},
    priority = 10,
    apply = { handler = "vfo_b_click" }
}

rule {
    id = "event-vfo-swap-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.VfoSwap"},
    priority = 10,
    apply = { handler = "vfo_swap_click" }
}

-- =============================================================================
-- Mode Button Clicks (generic set_value)
-- =============================================================================

rule {
    id = "event-mode-usb-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.ModeUSB"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "USB" }
}

rule {
    id = "event-mode-lsb-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.ModeLSB"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "LSB" }
}

rule {
    id = "event-mode-cw-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.ModeCW"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "CW" }
}

rule {
    id = "event-mode-am-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.ModeAM"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "AM" }
}

-- =============================================================================
-- DSP Checkbox Clicks (generic toggle_property)
-- =============================================================================

rule {
    id = "event-bandpass-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Checkbox", "widget.Bandpass"},
    priority = 10,
    apply = { handler = "toggle_property", property = "bandpassEnabled" }
}

rule {
    id = "event-notch-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Checkbox", "widget.Notch"},
    priority = 10,
    apply = { handler = "toggle_property", property = "notchEnabled" }
}

rule {
    id = "event-agc-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Checkbox", "widget.AGC"},
    priority = 10,
    apply = { handler = "toggle_property", property = "agcEnabled" }
}

rule {
    id = "event-nr-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Checkbox", "widget.NR"},
    priority = 10,
    apply = { handler = "toggle_property", property = "nrEnabled" }
}

rule {
    id = "event-nb-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Checkbox", "widget.NB"},
    priority = 10,
    apply = { handler = "toggle_property", property = "nbEnabled" }
}

-- =============================================================================
-- Audio Control Clicks (generic toggle_property)
-- =============================================================================

rule {
    id = "event-mute-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Checkbox", "widget.Mute"},
    priority = 10,
    apply = { handler = "toggle_property", property = "muteEnabled" }
}

rule {
    id = "event-test-tone-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.TestTone"},
    priority = 10,
    apply = { handler = "toggle_property", property = "testToneEnabled" }
}

rule {
    id = "event-wav-record-toggle",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.WavRecord"},
    priority = 10,
    apply = { handler = "wav_record_toggle" }
}

-- =============================================================================
-- RX Toggle Click
-- =============================================================================

rule {
    id = "event-rx-toggle-click",
    tags = {"event.MouseDown-LEFT", "widget.Toggle", "widget.RxToggle"},
    priority = 10,
    apply = { handler = "rx_toggle_click" }
}

-- =============================================================================
-- Band Button Clicks (generic set_value)
-- =============================================================================

rule {
    id = "event-band-160m-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.Band160m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "160m" }
}

rule {
    id = "event-band-80m-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.Band80m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "80m" }
}

rule {
    id = "event-band-40m-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.Band40m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "40m" }
}

rule {
    id = "event-band-20m-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.Band20m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "20m" }
}

rule {
    id = "event-band-15m-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.Band15m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "15m" }
}

rule {
    id = "event-band-10m-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.Band10m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "10m" }
}

-- =============================================================================
-- Colormap Button Clicks (generic set_value)
-- =============================================================================

rule {
    id = "event-cmap-viridis-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.CmapViridis"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "viridis" }
}

rule {
    id = "event-cmap-plasma-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.CmapPlasma"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "plasma" }
}

rule {
    id = "event-cmap-inferno-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.CmapInferno"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "inferno" }
}

rule {
    id = "event-cmap-green-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.CmapGreen"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "green" }
}

rule {
    id = "event-cmap-blue-click",
    tags = {"event.MouseDown-LEFT", "widget.Button", "widget.CmapBlue"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "blue" }
}

-- =============================================================================
-- Waterfall Click (for click-to-tune)
-- =============================================================================

rule {
    id = "event-waterfall-click",
    tags = {"event.MouseDown-LEFT", "widget.Waterfall"},
    apply = { handler = "waterfall_click" }
}

-- =============================================================================
-- Generic MouseUp Handlers (no-op to mark interaction complete)
-- =============================================================================

rule {
    id = "event-button-mouseup",
    tags = {"event.MouseUp-LEFT", "widget.Button"},
    apply = { handler = "noop" }
}

rule {
    id = "event-toggle-mouseup",
    tags = {"event.MouseUp-LEFT", "widget.Toggle"},
    apply = { handler = "noop" }
}

rule {
    id = "event-checkbox-mouseup",
    tags = {"event.MouseUp-LEFT", "widget.Checkbox"},
    apply = { handler = "noop" }
}

rule {
    id = "event-slider-mouseup",
    tags = {"event.MouseUp-LEFT", "widget.Slider"},
    apply = { handler = "noop" }
}

-- =============================================================================
-- UI Editing (Ctrl+Alt + mouse to resize/move/edit)
-- No mode - always available via modifier keys
-- =============================================================================

-- Ctrl+Alt+Left on widget edge = start resize
rule {
    id = "event-edit-resize-start",
    tags = {"event.MouseDown-LEFT", "input.CTRL", "input.ALT"},
    priority = 900,
    apply = { handler = "edit_resize_start" }
}

-- Ctrl+Alt+Middle = start move
rule {
    id = "event-edit-move-start",
    tags = {"event.MouseDown-MIDDLE", "input.CTRL", "input.ALT"},
    priority = 900,
    apply = { handler = "edit_move_start" }
}

-- Continue drag (resize or move) while Ctrl+Alt held
rule {
    id = "event-edit-drag-left",
    tags = {"event.MouseMove", "input.CTRL", "input.ALT", "input.MouseLEFT"},
    priority = 900,
    apply = { handler = "edit_drag" }
}

rule {
    id = "event-edit-drag-middle",
    tags = {"event.MouseMove", "input.CTRL", "input.ALT", "input.MouseMIDDLE"},
    priority = 900,
    apply = { handler = "edit_drag" }
}

-- End resize (left button release)
rule {
    id = "event-edit-resize-end",
    tags = {"event.MouseUp-LEFT", "input.CTRL", "input.ALT"},
    priority = 900,
    apply = { handler = "edit_drag_end" }
}

-- End move (middle button release)
rule {
    id = "event-edit-move-end",
    tags = {"event.MouseUp-MIDDLE", "input.CTRL", "input.ALT"},
    priority = 900,
    apply = { handler = "edit_drag_end" }
}

-- Ctrl+Alt+Right = context menu (for styling, delete, etc.)
rule {
    id = "event-edit-context-menu",
    tags = {"event.MouseDown-RIGHT", "input.CTRL", "input.ALT"},
    priority = 900,
    apply = { handler = "edit_context_menu" }
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
