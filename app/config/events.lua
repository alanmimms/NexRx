--[[
    NexRx Event Handler Rules

    Defines SetBox rules that map event + widget + modifier + mode combinations
    to handler function names. The events.lua module resolves these rules
    to dispatch events appropriately.

    Tag structure:
    - "Event" - Always present for event rules
    - Event type: "MouseDown", "MouseUp", "MouseMove", "MouseWheel", "KeyDown", "TextInput"
    - Button name: "Left", "Middle", "Right" (for mouse click/release events)
    - Key name: "Escape", "Enter", "F", "Q", etc. (for keyboard events)
    - Widget tags: "Button", "Checkbox", "Toggle", "Slider", etc.
    - Control tags: "VFOControl" (behavior tags - any widget with this tag tunes VFO)
    - Custom tags: "VfoA", "RxToggle", "ModeUSB", etc.
    - Mode tags: "FreqEntryMode" (added/removed dynamically)
    - Modifiers: "Shift", "Ctrl", "Alt" (keyboard) + "Left", "Middle", "Right" (held buttons for motion)

    Two-phase matching:
    1. First try with ALL tags (including modifiers) - most specific
    2. If no match, try WITHOUT modifier tags - general fallback

    Generic handlers use SetBox properties:
    - wheel_increment: property, step, step_ctrl, step_shift, step_ctrl_shift, min, max
    - set_value: property, value
    - toggle_property: property

    More specific rules (more tags, higher priority) take precedence.
]]

-- =============================================================================
-- Generic Slider Controls (wheel and arrow keys)
-- Handler gets min/max/property from widget.data, calculates steps as % of range
-- Steps: default=1%, ctrl=0.1%, shift=10%, ctrl+shift=25%
-- =============================================================================

-- Generic slider wheel (matches any Slider widget with property in widget.data)
rule {
    id = "event-slider-wheel",
    tags = {"Event", "MouseWheel", "Slider"},
    priority = 0,
    apply = { handler = "slider_adjust" }
}

-- Generic slider arrow keys
rule {
    id = "event-slider-arrow",
    tags = {"Event", "KeyDown", "Slider"},
    priority = 0,
    apply = { handler = "slider_adjust" }
}

-- Logarithmic slider (for LogScale tagged sliders like LMS mu)
-- Uses multiplicative factors: default=1.5x, ctrl=1.1x, shift=2x, ctrl+shift=5x
rule {
    id = "event-slider-log-wheel",
    tags = {"Event", "MouseWheel", "Slider", "LogScale"},
    priority = 10,
    apply = { handler = "slider_adjust_log" }
}

rule {
    id = "event-slider-log-arrow",
    tags = {"Event", "KeyDown", "Slider", "LogScale"},
    priority = 10,
    apply = { handler = "slider_adjust_log" }
}

-- =============================================================================
-- VFO Control (any widget tagged VFOControl responds to VFO tuning)
-- "Control" is a behavior tag - waterfall, spectrum, slider all tune VFO
-- Frequency is in MHz, so: 1kHz=0.001, 10kHz=0.01, 100kHz=0.1, 1MHz=1.0
-- =============================================================================

-- VFOControl base: 1 kHz steps (no modifiers)
rule {
    id = "event-vfo-wheel",
    tags = {"Event", "MouseWheel", "VFOControl"},
    priority = 20,
    apply = { handler = "vfo_control", step = 0.001 }
}

rule {
    id = "event-vfo-arrow",
    tags = {"Event", "KeyDown", "VFOControl"},
    priority = 20,
    apply = { handler = "vfo_control", step = 0.001 }
}

-- VFOControl + Ctrl: 10 kHz steps
rule {
    id = "event-vfo-wheel-ctrl",
    tags = {"Event", "MouseWheel", "VFOControl", "Ctrl"},
    priority = 30,
    apply = { handler = "vfo_control", step = 0.01 }
}

rule {
    id = "event-vfo-arrow-ctrl",
    tags = {"Event", "KeyDown", "VFOControl", "Ctrl"},
    priority = 30,
    apply = { handler = "vfo_control", step = 0.01 }
}

-- VFOControl + Shift: 1 MHz steps
rule {
    id = "event-vfo-wheel-shift",
    tags = {"Event", "MouseWheel", "VFOControl", "Shift"},
    priority = 30,
    apply = { handler = "vfo_control", step = 1.0 }
}

rule {
    id = "event-vfo-arrow-shift",
    tags = {"Event", "KeyDown", "VFOControl", "Shift"},
    priority = 30,
    apply = { handler = "vfo_control", step = 1.0 }
}

-- VFOControl + Ctrl + Shift: 100 kHz steps
rule {
    id = "event-vfo-wheel-ctrl-shift",
    tags = {"Event", "MouseWheel", "VFOControl", "Ctrl", "Shift"},
    priority = 40,
    apply = { handler = "vfo_control", step = 0.1 }
}

rule {
    id = "event-vfo-arrow-ctrl-shift",
    tags = {"Event", "KeyDown", "VFOControl", "Ctrl", "Shift"},
    priority = 40,
    apply = { handler = "vfo_control", step = 0.1 }
}

-- VFOControl + H: 10 Hz fine tuning
rule {
    id = "event-vfo-wheel-h",
    tags = {"Event", "MouseWheel", "VFOControl", "H"},
    priority = 30,
    apply = { handler = "vfo_control", step = 0.00001 }
}

rule {
    id = "event-vfo-arrow-h",
    tags = {"Event", "KeyDown", "VFOControl", "H"},
    priority = 30,
    apply = { handler = "vfo_control", step = 0.00001 }
}

-- VFOControl + Shift + H: 100 Hz fine tuning
rule {
    id = "event-vfo-wheel-shift-h",
    tags = {"Event", "MouseWheel", "VFOControl", "Shift", "H"},
    priority = 40,
    apply = { handler = "vfo_control", step = 0.0001 }
}

rule {
    id = "event-vfo-arrow-shift-h",
    tags = {"Event", "KeyDown", "VFOControl", "Shift", "H"},
    priority = 40,
    apply = { handler = "vfo_control", step = 0.0001 }
}

-- KeyUp on VFOControl widgets - silently consume (keys used as modifiers)
rule {
    id = "event-vfo-keyup",
    tags = {"Event", "KeyUp", "VFOControl"},
    priority = 0,
    apply = { handler = "noop" }
}

-- =============================================================================
-- Frequency Entry Mode
-- =============================================================================

rule {
    id = "event-freq-entry-start-f",
    tags = {"Event", "KeyDown", "F"},
    priority = 0,
    apply = { handler = "freq_entry_start" }
}

rule {
    id = "event-freq-entry-start-click",
    tags = {"Event", "MouseDown", "Left", "FrequencyDisplay"},
    priority = 0,
    apply = { handler = "freq_entry_start" }
}

rule {
    id = "event-freq-entry-cancel",
    tags = {"Event", "KeyDown", "Escape", "FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_cancel" }
}

rule {
    id = "event-freq-entry-confirm",
    tags = {"Event", "KeyDown", "Enter", "FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_confirm" }
}

rule {
    id = "event-freq-entry-backspace",
    tags = {"Event", "KeyDown", "Backspace", "FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_backspace" }
}

rule {
    id = "event-freq-entry-text",
    tags = {"Event", "TextInput", "FreqEntryMode"},
    priority = 100,
    apply = { handler = "freq_entry_text" }
}

-- =============================================================================
-- Application Control (Global)
-- =============================================================================

rule {
    id = "event-app-quit",
    tags = {"Event", "KeyDown", "Q", "Ctrl"},
    priority = 1000,
    apply = { handler = "app_quit" }
}

rule {
    id = "event-debug-toggle",
    tags = {"Event", "KeyDown", "D", "Ctrl"},
    priority = 1000,
    apply = { handler = "debug_toggle" }
}

-- =============================================================================
-- VFO Button Clicks (specific logic, keep dedicated handlers)
-- =============================================================================

rule {
    id = "event-vfo-a-click",
    tags = {"Event", "MouseDown", "Left", "Button", "VfoA"},
    priority = 10,
    apply = { handler = "vfo_a_click" }
}

rule {
    id = "event-vfo-b-click",
    tags = {"Event", "MouseDown", "Left", "Button", "VfoB"},
    priority = 10,
    apply = { handler = "vfo_b_click" }
}

rule {
    id = "event-vfo-swap-click",
    tags = {"Event", "MouseDown", "Left", "Button", "VfoSwap"},
    priority = 10,
    apply = { handler = "vfo_swap_click" }
}

-- =============================================================================
-- Mode Button Clicks (generic set_value)
-- =============================================================================

rule {
    id = "event-mode-usb-click",
    tags = {"Event", "MouseDown", "Left", "Button", "ModeUSB"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "USB" }
}

rule {
    id = "event-mode-lsb-click",
    tags = {"Event", "MouseDown", "Left", "Button", "ModeLSB"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "LSB" }
}

rule {
    id = "event-mode-cw-click",
    tags = {"Event", "MouseDown", "Left", "Button", "ModeCW"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "CW" }
}

rule {
    id = "event-mode-am-click",
    tags = {"Event", "MouseDown", "Left", "Button", "ModeAM"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedMode", value = "AM" }
}

-- =============================================================================
-- DSP Checkbox Clicks (generic toggle_property)
-- =============================================================================

rule {
    id = "event-bandpass-toggle",
    tags = {"Event", "MouseDown", "Left", "Checkbox", "Bandpass"},
    priority = 10,
    apply = { handler = "toggle_property", property = "bandpassEnabled" }
}

rule {
    id = "event-notch-toggle",
    tags = {"Event", "MouseDown", "Left", "Checkbox", "Notch"},
    priority = 10,
    apply = { handler = "toggle_property", property = "notchEnabled" }
}

rule {
    id = "event-agc-toggle",
    tags = {"Event", "MouseDown", "Left", "Checkbox", "AGC"},
    priority = 10,
    apply = { handler = "toggle_property", property = "agcEnabled" }
}

rule {
    id = "event-nr-toggle",
    tags = {"Event", "MouseDown", "Left", "Checkbox", "NR"},
    priority = 10,
    apply = { handler = "toggle_property", property = "nrEnabled" }
}

rule {
    id = "event-nb-toggle",
    tags = {"Event", "MouseDown", "Left", "Checkbox", "NB"},
    priority = 10,
    apply = { handler = "toggle_property", property = "nbEnabled" }
}

-- =============================================================================
-- Audio Control Clicks (generic toggle_property)
-- =============================================================================

rule {
    id = "event-mute-toggle",
    tags = {"Event", "MouseDown", "Left", "Checkbox", "Mute"},
    priority = 10,
    apply = { handler = "toggle_property", property = "muteEnabled" }
}

rule {
    id = "event-test-tone-toggle",
    tags = {"Event", "MouseDown", "Left", "Button", "TestTone"},
    priority = 10,
    apply = { handler = "toggle_property", property = "testToneEnabled" }
}

rule {
    id = "event-wav-record-toggle",
    tags = {"Event", "MouseDown", "Left", "Button", "WavRecord"},
    priority = 10,
    apply = { handler = "wav_record_toggle" }
}

-- =============================================================================
-- RX Toggle Click
-- =============================================================================

rule {
    id = "event-rx-toggle-click",
    tags = {"Event", "MouseDown", "Left", "Toggle", "RxToggle"},
    priority = 10,
    apply = { handler = "rx_toggle_click" }
}

-- =============================================================================
-- Band Button Clicks (generic set_value)
-- =============================================================================

rule {
    id = "event-band-160m-click",
    tags = {"Event", "MouseDown", "Left", "Button", "Band160m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "160m" }
}

rule {
    id = "event-band-80m-click",
    tags = {"Event", "MouseDown", "Left", "Button", "Band80m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "80m" }
}

rule {
    id = "event-band-40m-click",
    tags = {"Event", "MouseDown", "Left", "Button", "Band40m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "40m" }
}

rule {
    id = "event-band-20m-click",
    tags = {"Event", "MouseDown", "Left", "Button", "Band20m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "20m" }
}

rule {
    id = "event-band-15m-click",
    tags = {"Event", "MouseDown", "Left", "Button", "Band15m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "15m" }
}

rule {
    id = "event-band-10m-click",
    tags = {"Event", "MouseDown", "Left", "Button", "Band10m"},
    priority = 10,
    apply = { handler = "set_value", property = "selectedBand", value = "10m" }
}

-- =============================================================================
-- Colormap Button Clicks (generic set_value)
-- =============================================================================

rule {
    id = "event-cmap-viridis-click",
    tags = {"Event", "MouseDown", "Left", "Button", "CmapViridis"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "viridis" }
}

rule {
    id = "event-cmap-plasma-click",
    tags = {"Event", "MouseDown", "Left", "Button", "CmapPlasma"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "plasma" }
}

rule {
    id = "event-cmap-inferno-click",
    tags = {"Event", "MouseDown", "Left", "Button", "CmapInferno"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "inferno" }
}

rule {
    id = "event-cmap-green-click",
    tags = {"Event", "MouseDown", "Left", "Button", "CmapGreen"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "green" }
}

rule {
    id = "event-cmap-blue-click",
    tags = {"Event", "MouseDown", "Left", "Button", "CmapBlue"},
    priority = 10,
    apply = { handler = "set_value", property = "wfColormap", value = "blue" }
}

-- =============================================================================
-- Waterfall Click (for click-to-tune)
-- =============================================================================

rule {
    id = "event-waterfall-click",
    tags = {"Event", "MouseDown", "Left", "Waterfall"},
    priority = 0,
    apply = { handler = "waterfall_click" }
}

-- =============================================================================
-- Generic MouseUp Handlers (no-op to mark interaction complete)
-- =============================================================================

rule {
    id = "event-button-mouseup",
    tags = {"Event", "MouseUp", "Left", "Button"},
    priority = 0,
    apply = { handler = "noop" }
}

rule {
    id = "event-toggle-mouseup",
    tags = {"Event", "MouseUp", "Left", "Toggle"},
    priority = 0,
    apply = { handler = "noop" }
}

rule {
    id = "event-checkbox-mouseup",
    tags = {"Event", "MouseUp", "Left", "Checkbox"},
    priority = 0,
    apply = { handler = "noop" }
}

rule {
    id = "event-slider-mouseup",
    tags = {"Event", "MouseUp", "Left", "Slider"},
    priority = 0,
    apply = { handler = "noop" }
}

-- =============================================================================
-- Fallback: Unhandled Events
-- =============================================================================

rule {
    id = "event-unhandled",
    tags = {"Event", "Unhandled"},
    priority = -1000,
    apply = { handler = "log_unhandled" }
}

rule {
    id = "event-default",
    tags = {"Event"},
    priority = -1000,
    apply = { handler = nil }
}

print("[events.lua] Event handler rules loaded")
