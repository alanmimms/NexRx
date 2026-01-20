--[[
    NexRx Event Handler Rules

    Defines SetBox rules that map event + widget + modifier + mode combinations
    to handler function names. The events.lua module resolves these rules
    to dispatch events appropriately.

    Tag structure:
    - "Event" - Always present for event rules
    - Event type: "MouseDown", "MouseUp", "MouseWheel", "KeyDown", "TextInput"
    - Key name: "Escape", "Enter", "F", "Q", etc. (for keyboard events)
    - Widget tags: "Button", "Checkbox", "Toggle", "Slider", etc.
    - Custom tags: "VfoA", "RxToggle", "ModeUSB", etc.
    - Mode tags: "FreqEntryMode" (added/removed dynamically)
    - Modifiers: "Shift", "Ctrl", "Alt"

    More specific rules (more tags, higher priority) take precedence.
    Handler names reference functions registered via events.registerHandler().
]]

-- =============================================================================
-- VFO Tuning (Mouse Wheel on Frequency Display)
-- =============================================================================

rule {
    id = "event-vfo-wheel",
    tags = {"Event", "MouseWheel", "FrequencyDisplay"},
    priority = 0,
    apply = { handler = "vfo_tune" }
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
    tags = {"Event", "MouseDown", "FrequencyDisplay"},
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
-- Application Control
-- =============================================================================

rule {
    id = "event-app-quit",
    tags = {"Event", "KeyDown", "Q", "Ctrl"},
    priority = 1000,
    apply = { handler = "app_quit" }
}

-- =============================================================================
-- VFO Button Clicks
-- =============================================================================

rule {
    id = "event-vfo-a-click",
    tags = {"Event", "MouseDown", "Button", "VfoA"},
    priority = 10,
    apply = { handler = "vfo_a_click" }
}

rule {
    id = "event-vfo-b-click",
    tags = {"Event", "MouseDown", "Button", "VfoB"},
    priority = 10,
    apply = { handler = "vfo_b_click" }
}

rule {
    id = "event-vfo-swap-click",
    tags = {"Event", "MouseDown", "Button", "VfoSwap"},
    priority = 10,
    apply = { handler = "vfo_swap_click" }
}

-- =============================================================================
-- Mode Button Clicks
-- =============================================================================

rule {
    id = "event-mode-usb-click",
    tags = {"Event", "MouseDown", "Button", "ModeUSB"},
    priority = 10,
    apply = { handler = "mode_usb_click" }
}

rule {
    id = "event-mode-lsb-click",
    tags = {"Event", "MouseDown", "Button", "ModeLSB"},
    priority = 10,
    apply = { handler = "mode_lsb_click" }
}

rule {
    id = "event-mode-cw-click",
    tags = {"Event", "MouseDown", "Button", "ModeCW"},
    priority = 10,
    apply = { handler = "mode_cw_click" }
}

rule {
    id = "event-mode-am-click",
    tags = {"Event", "MouseDown", "Button", "ModeAM"},
    priority = 10,
    apply = { handler = "mode_am_click" }
}

-- =============================================================================
-- DSP Checkbox Clicks
-- =============================================================================

rule {
    id = "event-bandpass-toggle",
    tags = {"Event", "MouseDown", "Checkbox", "Bandpass"},
    priority = 10,
    apply = { handler = "bandpass_toggle" }
}

rule {
    id = "event-notch-toggle",
    tags = {"Event", "MouseDown", "Checkbox", "Notch"},
    priority = 10,
    apply = { handler = "notch_toggle" }
}

rule {
    id = "event-agc-toggle",
    tags = {"Event", "MouseDown", "Checkbox", "AGC"},
    priority = 10,
    apply = { handler = "agc_toggle" }
}

rule {
    id = "event-nr-toggle",
    tags = {"Event", "MouseDown", "Checkbox", "NR"},
    priority = 10,
    apply = { handler = "nr_toggle" }
}

rule {
    id = "event-nb-toggle",
    tags = {"Event", "MouseDown", "Checkbox", "NB"},
    priority = 10,
    apply = { handler = "nb_toggle" }
}

-- =============================================================================
-- Audio Control Clicks
-- =============================================================================

rule {
    id = "event-mute-toggle",
    tags = {"Event", "MouseDown", "Checkbox", "Mute"},
    priority = 10,
    apply = { handler = "mute_toggle" }
}

rule {
    id = "event-test-tone-toggle",
    tags = {"Event", "MouseDown", "Button", "TestTone"},
    priority = 10,
    apply = { handler = "test_tone_toggle" }
}

rule {
    id = "event-wav-record-toggle",
    tags = {"Event", "MouseDown", "Button", "WavRecord"},
    priority = 10,
    apply = { handler = "wav_record_toggle" }
}

-- =============================================================================
-- RX Toggle Click
-- =============================================================================

rule {
    id = "event-rx-toggle-click",
    tags = {"Event", "MouseDown", "Toggle", "RxToggle"},
    priority = 10,
    apply = { handler = "rx_toggle_click" }
}

-- =============================================================================
-- Band Button Clicks
-- =============================================================================

rule {
    id = "event-band-160m-click",
    tags = {"Event", "MouseDown", "Button", "Band160m"},
    priority = 10,
    apply = { handler = "band_160m_click" }
}

rule {
    id = "event-band-80m-click",
    tags = {"Event", "MouseDown", "Button", "Band80m"},
    priority = 10,
    apply = { handler = "band_80m_click" }
}

rule {
    id = "event-band-40m-click",
    tags = {"Event", "MouseDown", "Button", "Band40m"},
    priority = 10,
    apply = { handler = "band_40m_click" }
}

rule {
    id = "event-band-20m-click",
    tags = {"Event", "MouseDown", "Button", "Band20m"},
    priority = 10,
    apply = { handler = "band_20m_click" }
}

rule {
    id = "event-band-15m-click",
    tags = {"Event", "MouseDown", "Button", "Band15m"},
    priority = 10,
    apply = { handler = "band_15m_click" }
}

rule {
    id = "event-band-10m-click",
    tags = {"Event", "MouseDown", "Button", "Band10m"},
    priority = 10,
    apply = { handler = "band_10m_click" }
}

-- =============================================================================
-- Colormap Button Clicks
-- =============================================================================

rule {
    id = "event-cmap-viridis-click",
    tags = {"Event", "MouseDown", "Button", "CmapViridis"},
    priority = 10,
    apply = { handler = "cmap_viridis_click" }
}

rule {
    id = "event-cmap-plasma-click",
    tags = {"Event", "MouseDown", "Button", "CmapPlasma"},
    priority = 10,
    apply = { handler = "cmap_plasma_click" }
}

rule {
    id = "event-cmap-inferno-click",
    tags = {"Event", "MouseDown", "Button", "CmapInferno"},
    priority = 10,
    apply = { handler = "cmap_inferno_click" }
}

rule {
    id = "event-cmap-green-click",
    tags = {"Event", "MouseDown", "Button", "CmapGreen"},
    priority = 10,
    apply = { handler = "cmap_green_click" }
}

rule {
    id = "event-cmap-blue-click",
    tags = {"Event", "MouseDown", "Button", "CmapBlue"},
    priority = 10,
    apply = { handler = "cmap_blue_click" }
}

-- =============================================================================
-- Spectrum/Waterfall Interactions (mouse wheel tunes VFO)
-- =============================================================================

rule {
    id = "event-spectrum-wheel",
    tags = {"Event", "MouseWheel", "Spectrum"},
    priority = 0,
    apply = { handler = "vfo_tune" }
}

rule {
    id = "event-waterfall-wheel",
    tags = {"Event", "MouseWheel", "Waterfall"},
    priority = 0,
    apply = { handler = "vfo_tune" }
}

rule {
    id = "event-waterfall-click",
    tags = {"Event", "MouseDown", "Waterfall"},
    priority = 0,
    apply = { handler = "waterfall_click" }
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
