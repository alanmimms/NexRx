--[[
  NexRx Default Configuration

  This file defines all default SetBox rules shipped with the application.
  Users can override any of these settings in config/settings.lua.

  Priority levels:
    -100 to -1: Base defaults (easily overridden)
       0 to 50: Normal user rules
      51 to 99: User overrides
     100+     : Experimental/debug overrides
]]

-- =============================================================================
-- Global Defaults (applies when no other rules match)
-- =============================================================================

rule {
    id = "global-defaults",
    tags = {},
    priority = -100,
    apply = {
        -- =================================================================
        -- Radio Defaults
        -- =================================================================
        ["rx.VFO.active"] = "A",
        ["rx.VFO.A"] = 14.200e6,         -- Hz
        ["rx.VFO.B"] = 7.050e6,          -- Hz
        ["rx.selectedMode"] = "USB",     -- Current mode
        ["rx.selectedBand"] = "20m",     -- Current band
        ["rx.active"] = true,

        -- =================================================================
        -- DSP Defaults
        -- =================================================================
        ["rx.squelch"] = 0.3,
        ["rx.AGC.enabled"] = false,
        ["rx.AGC.mode"] = 0,
        ["rx.NR.enabled"] = false,
        ["rx.NB.enabled"] = false,
        ["rx.lmsMu"] = 0.5,
        ["rx.BFO.offset"] = 700,
        ["rx.volume.DB"] = -20,
        ["rx.volume.muted"] = false,
        ["rx.testToneEnabled"] = false,
        ["rx.demodFilterEnabled"] = true,
        
        -- Animation
        animated = false,
        
        -- Filter Defaults
        ["rx.bandpass.enabled"] = false,
        ["rx.bandpass.center"] = 700,
        ["rx.bandpass.width"] = 500,
        ["rx.notch.enabled"] = false,
        ["rx.notch.center"] = 0,
        ["rx.notch.width"] = 100,

        -- =================================================================
        -- Hardware Defaults
        -- =================================================================
        ["rx.QSD.offsetK"] = 12.0,      -- kHz
        ["rx.RF.attenuationDB"] = 0,    -- DB (0-45 in 3 DB steps)
        ["rx.RF.gainDB"] = 20.0,        -- DB (Digital gain)

        -- Preselector
        ["preselector.L"] = false,
        ["preselector.capMask"] = 0,
        ["preselector.auto"] = true,
        ["preselector.enabled"] = true,

        -- Internal Signal Generator
        isgEnabled = false,
        isgFrequency = 14.201e6,        -- Hz (1 kHz offset from default frequency)

        -- =================================================================
        -- Display Defaults
        -- =================================================================
        wfBins = 512,
        wfRows = 256,
        wfColormap = "viridis",
        wfMinDB = -140,
        wfMaxDB = 0,
        spectrumEmaAlpha = 0.3,         -- Spectrum smoothing

        -- =================================================================
        -- Window Defaults
        -- =================================================================
        windowWidth = 1900,  -- Widened to accommodate Active Tags debug widget
        windowHeight = 850,
        fontSize = 16,

        -- Fallback font (bundled with app)
        fontPaths = { "fonts/DejaVuSans.ttf" },

        -- =================================================================
        -- Layout Defaults (Core System)
        -- =================================================================
        anchorLeft = 0, anchorRight = 0, anchorTop = 0, anchorBottom = 0,
        anchor = "top", group = "",
        width = 0, height = 0, 
        minWidth = 0, minHeight = 0, maxWidth = 9999, maxHeight = 9999,
        marginInner = 0, marginOuter = 0,
        springX = 0, springY = 0,
        order = 0,
        parent = "",
        fallbackPadding = 8,
        fallbackSpacing = 4,
        fallbackLineHeight = 20,

        -- =================================================================
        -- Connection Defaults (hw abstraction layer)
        -- =================================================================
        hwHost = "127.0.0.1",           -- Hardware/twin server address
        hwControlPort = 5000,           -- TCP control port
        hwStreamPort = 5001,            -- UDP stream port
        hwAutoConnect = true,           -- Auto-connect on startup

        -- =================================================================
        -- Recording Defaults
        -- =================================================================
        recordingPath = "/tmp/nexrx_audio.wav",

        -- =================================================================
        -- Audio Defaults
        -- =================================================================
        volumeDb = -20,             -- dB, range -60 to 0
        muted = false,

        -- =================================================================
        -- UI Theme Colors
        -- =================================================================
        background = "#1a1a2e",
        foreground = "#e2e8f0",
        accent = "#3b82f6",
        border = "#4a5568",
        borderWidth = 1,

        -- Typography
        fontFamily = "system-ui",

        -- Spacing
        padding = 8,
        margin = 4,
        borderRadius = 4,

        -- =================================================================
        -- Animation Defaults
        -- =================================================================
        anim_duration = 0.2,
        anim_easing = "easeInOut",
    }
}

-- Platform-specific font paths
rule {
    id = "fonts-windows",
    tags = {"platform.Windows"},
    priority = -90,
    apply = {
        fontPaths = {
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/tahoma.ttf",
            "fonts/DejaVuSans.ttf",
        }
    }
}

rule {
    id = "fonts-macos",
    tags = {"platform.macOS"},
    priority = -90,
    apply = {
        fontPaths = {
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/Library/Fonts/Arial.ttf",
            "fonts/DejaVuSans.ttf",
        }
    }
}

rule {
    id = "fonts-linux",
    tags = {"platform.Linux"},
    priority = -90,
    apply = {
        fontPaths = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "fonts/DejaVuSans.ttf",
        }
    }
}

-- Property-specific animation overrides
rule {
    id = "audio-animation",
    tags = {"prop.volumeDb"},
    priority = -90,
    apply = {
        animated = true,
        anim_duration = 0.3,
        anim_easing = "easeOut",
    }
}

rule {
    id = "filter-animation",
    tags = {"prop.bandpassWidth", "prop.bandpassCenter", "prop.notchWidth", "prop.notchCenter"},
    priority = -90,
    apply = {
        animated = true,
        anim_duration = 0.15,
        anim_easing = "easeInOut",
    }
}

-- =============================================================================
-- UI Component Styles
-- =============================================================================

-- Button base
rule {
    id = "button-base",
    tags = {"widget.Button"},
    apply = {
        background = "#3b82f6",
        foreground = "#ffffff",
        padding = 8,
        borderRadius = 6,
        cursor = "pointer",
    }
}

rule {
    id = "button-primary",
    tags = {"widget.Button", "widget.Primary"},
    apply = {
        background = "#2563eb",
        fontWeight = "bold",
    }
}

rule {
    id = "button-secondary",
    tags = {"widget.Button", "widget.Secondary"},
    apply = {
        background = "#64748b",
    }
}

rule {
    id = "button-danger",
    tags = {"widget.Button", "widget.Danger"},
    apply = {
        background = "#dc2626",
    }
}

-- RX Toggle styling (theme queries with "widget.Button" prefix + passed tags)
rule {
    id = "rx-toggle-off",
    tags = {"widget.Button", "widget.RxToggle"},
    priority = 15,
    apply = {
        background = "#dc2626",  -- Red when OFF
    }
}

rule {
    id = "rx-toggle-on",
    tags = {"widget.Button", "widget.RxToggle", "state.Active"},
    priority = 16,  -- Higher priority to override OFF state
    apply = {
        background = "#16a34a",  -- Green when ON
    }
}

-- Button state styles (tag-based, used by immediate-mode UI)
rule {
    id = "button-hovered",
    tags = {"widget.Button", "state.Hovered"},
    priority = 10,
    apply = {
        background = "#60a5fa",
        border = "#93c5fd",
    }
}

rule {
    id = "button-pressed",
    tags = {"widget.Button", "state.Pressed"},
    priority = 11,
    apply = {
        background = "#1d4ed8",
        border = "#60a5fa",
    }
}

rule {
    id = "button-active",
    tags = {"widget.Button", "state.Active"},
    priority = 5,
    apply = {
        background = "#2563eb",
        border = "#60a5fa",
    }
}

rule {
    id = "button-disabled",
    tags = {"widget.Button", "state.Disabled"},
    priority = 20,
    apply = {
        background = "#4a5568",
        foreground = "#a0aec0",
    }
}

-- Panel styles
rule {
    id = "panel-base",
    tags = {"widget.Panel"},
    apply = {
        background = "#1e293b",
        borderRadius = 8,
        padding = 12,
    }
}

rule {
    id = "panel-sidebar",
    tags = {"widget.Panel", "widget.Sidebar"},
    apply = {
        background = "#0f172a",
        borderRadius = 0,
        width = 280,
    }
}

rule {
    id = "preselector-frame",
    tags = {"widget.Panel", "widget.PreselectorFrame"},
    apply = {
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
    }
}

rule {
    id = "isg-frame",
    tags = {"widget.Panel", "widget.ISGFrame"},
    apply = {
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
    }
}

rule {
    id = "agc-frame",
    tags = {"widget.Panel", "widget.AGCFrame"},
    apply = {
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
    }
}

rule {
    id = "audio-utils-frame",
    tags = {"widget.Panel", "widget.AudioUtilsFrame"},
    apply = {
        background = "#242d42",
        border = "#3b82f6",
        borderRadius = 8,
    }
}

-- Sidebar Widgets Ordering
rule { id = "id-isg",   tags = {"id.isg"},   apply = { parent = "right-sidebar", order = 10, height = 160, title = "SIGNAL GEN" } }
rule { id = "id-presel",tags = {"id.presel"},apply = { parent = "right-sidebar", order = 20, height = 180, title = "PRESELECTOR" } }
rule { id = "id-agc",   tags = {"id.agc"},   apply = { parent = "right-sidebar", order = 30, height = 80, title = "AGC" } }
rule { id = "id-audio", tags = {"id.audio"}, apply = { parent = "right-sidebar", order = 40, height = 140, title = "AUDIO UTILS" } }

-- Slider styles
rule {
    id = "slider-base",
    tags = {"widget.Slider"},
    apply = {
        background = "#2d3748",
        accent = "#3b82f6",
        border = "#4a5568",
    }
}

rule {
    id = "slider-hovered",
    tags = {"widget.Slider", "state.Hovered"},
    priority = 10,
    apply = {
        accent = "#60a5fa",
    }
}

rule {
    id = "slider-active",
    tags = {"widget.Slider", "state.Active"},
    priority = 11,
    apply = {
        accent = "#2563eb",
    }
}

-- Checkbox styles
rule {
    id = "checkbox-base",
    tags = {"widget.Checkbox"},
    apply = {
        background = "#2d3748",
        foreground = "#e2e8f0",
        border = "#4a5568",
        accent = "#3b82f6",
    }
}

rule {
    id = "checkbox-checked",
    tags = {"widget.Checkbox", "state.Checked"},
    priority = 5,
    apply = {
        background = "#3b82f6",
        border = "#60a5fa",
    }
}

rule {
    id = "checkbox-hovered",
    tags = {"widget.Checkbox", "state.Hovered"},
    priority = 10,
    apply = {
        border = "#60a5fa",
    }
}

-- Input field styles
rule {
    id = "input-base",
    tags = {"widget.Input"},
    apply = {
        background = "#1e293b",
        foreground = "#e2e8f0",
        border = "#4a5568",
        borderRadius = 4,
        accent = "#3b82f6",
    }
}

rule {
    id = "input-hovered",
    tags = {"widget.Input", "state.Hovered"},
    priority = 10,
    apply = {
        border = "#60a5fa",
    }
}

rule {
    id = "input-focused",
    tags = {"widget.Input", "state.Focused"},
    priority = 11,
    apply = {
        border = "#3b82f6",
        borderWidth = 2,
    }
}

-- Label styles
rule {
    id = "label-base",
    tags = {"widget.Label"},
    apply = {
        foreground = "#e2e8f0",
    }
}

rule {
    id = "label-title",
    tags = {"widget.Label", "widget.Title"},
    apply = {
        foreground = "#f1f5f9",
        fontSize = 16,
    }
}

rule {
    id = "label-muted",
    tags = {"widget.Label", "widget.Muted"},
    apply = {
        foreground = "#94a3b8",
    }
}

rule {
    id = "label-accent",
    tags = {"widget.Label", "widget.Accent"},
    apply = {
        foreground = "#60a5fa",
    }
}

-- Progress bar styles
rule {
    id = "progressbar-base",
    tags = {"widget.ProgressBar"},
    apply = {
        background = "#2d3748",
        accent = "#3b82f6",
    }
}

-- Separator styles
rule {
    id = "separator-base",
    tags = {"widget.Separator"},
    apply = {
        border = "#4a5568",
    }
}

-- S-meter colors
rule {
    id = "smeter-style",
    tags = {"widget.SMeter"},
    apply = {
        background = "#1e293b",
        colorWeak = "#22c55e",   -- Green (S1-S5)
        colorMid = "#eab308",    -- Yellow (S6-S9)
        colorStrong = "#ef4444", -- Red (S9+)
        colorOff = "#334155",    -- Dark blue-gray (inactive)
    }
}

-- =============================================================================
-- Radio Configuration Defaults
-- =============================================================================

-- Band defaults
rule {
    id = "radio-160m",
    tags = {"widget.Radio", "state.Band-160m"},
    apply = {
        frequency = 1.9e6,
        antenna = "wire",
    }
}

rule {
    id = "radio-80m",
    tags = {"widget.Radio", "state.Band-80m"},
    apply = {
        frequency = 3.5e6,
        antenna = "dipole",
    }
}

rule {
    id = "radio-40m",
    tags = {"widget.Radio", "state.Band-40m"},
    apply = {
        frequency = 7.0e6,
        antenna = "dipole",
    }
}

rule {
    id = "radio-20m",
    tags = {"widget.Radio", "state.Band-20m"},
    apply = {
        frequency = 14.0e6,
        antenna = "beam",
    }
}

rule {
    id = "radio-15m",
    tags = {"widget.Radio", "state.Band-15m"},
    apply = {
        frequency = 21.0e6,
        antenna = "beam",
    }
}

rule {
    id = "radio-10m",
    tags = {"widget.Radio", "state.Band-10m"},
    apply = {
        frequency = 28.0e6,
        antenna = "beam",
    }
}

-- Mode defaults
rule {
    id = "radio-cw",
    tags = {"widget.Radio", "state.Mode-CW"},
    apply = {
        mode = "CW",
        filterWidth = 500,
        sidetoneFreq = 600,
        agcMode = "fast",
    }
}

rule {
    id = "radio-ssb",
    tags = {"widget.Radio", "state.Mode-SSB"},
    apply = {
        mode = "USB",
        filterWidth = 2400,
        agcMode = "medium",
    }
}

rule {
    id = "radio-lsb",
    tags = {"widget.Radio", "state.Mode-LSB"},
    apply = {
        mode = "LSB",
    }
}

rule {
    id = "radio-usb",
    tags = {"widget.Radio", "state.Mode-USB"},
    apply = {
        mode = "USB",
    }
}

rule {
    id = "radio-am",
    tags = {"widget.Radio", "state.Mode-AM"},
    apply = {
        mode = "AM",
        filterWidth = 6000,
        agcMode = "slow",
    }
}

-- Band + Mode combinations (more specific)
rule {
    id = "radio-40m-ssb",
    tags = {"widget.Radio", "state.Band-40m", "state.Mode-SSB"},
    apply = {
        frequency = 7.2e6,
        mode = "LSB",  -- 40m and below use LSB
    }
}

rule {
    id = "radio-20m-cw",
    tags = {"widget.Radio", "state.Band-20m", "state.Mode-CW"},
    apply = {
        frequency = 14.035e6,
    }
}

rule {
    id = "radio-20m-ssb",
    tags = {"widget.Radio", "state.Band-20m", "state.Mode-SSB"},
    apply = {
        frequency = 14.2e6,
        mode = "USB",
    }
}

-- =============================================================================
-- Waterfall Display
-- =============================================================================

rule {
    id = "waterfall-base",
    tags = {"widget.Waterfall"},
    apply = {
        wfColormap = "viridis",
        speed = 50,
        gain = 0,
        minDb = -120,
        maxDb = -40,
    }
}

rule {
    id = "waterfall-cw",
    tags = {"widget.Waterfall", "state.Mode-CW"},
    apply = {
        wfColormap = "green_phosphor",
        span = 3000,
    }
}

rule {
    id = "waterfall-ssb",
    tags = {"widget.Waterfall", "state.Mode-SSB"},
    apply = {
        span = 6000,
    }
}

-- =============================================================================
-- Contest Mode
-- =============================================================================

rule {
    id = "contest-base",
    tags = {"state.Contest"},
    apply = {
        -- Compact UI
        fontSize = 12,
        padding = 4,

        -- Quick AGC
        agcMode = "fast",
    }
}

rule {
    id = "contest-cw",
    tags = {"state.Contest", "state.Mode-CW"},
    apply = {
        filterWidth = 400,
        sidetoneFreq = 550,
    }
}

rule {
    id = "agc-mode-button",
    tags = {"widget.Button", "widget.AgcMode"},
    priority = 10,
    apply = {
        background = "#1e293b",
        foreground = "#94a3b8",
    }
}

rule {
    id = "agc-mode-active",
    tags = {"widget.Button", "widget.AgcMode", "state.Active"},
    priority = 20,
    apply = {
        background = "#3b82f6",
        foreground = "#ffffff",
    }
}

-- Graticule Legend
rule {
    id = "graticule-legend",
    tags = {"widget.GraticuleLegend"},
    apply = {
        background = "#0f172a",
        foreground = "#94a3b8",
        width = 100,
        height = 45,
        padding = 6,
    }
}

-- Sidebar Content Labels
rule { tags = {"id.mode-label"}, apply = { text = "Mode" } }
rule { tags = {"id.band-label"}, apply = { text = "Band" } }
rule { tags = {"id.vol-label"},  apply = { text = "Volume" } }
rule { tags = {"id.rfg-label"},  apply = { text = "RF Gain" } }
rule { tags = {"id.sm-label"},   apply = { text = "S-Meter" } }

-- Mode Button Labels
rule { tags = {"id.mode-usb"},    apply = { label = "USB" } }
rule { tags = {"id.mode-lsb"},    apply = { label = "LSB" } }
rule { tags = {"id.mode-cw"},     apply = { label = "CW" } }
rule { tags = {"id.mode-am"},     apply = { label = "AM" } }
rule { tags = {"id.mode-bypass"}, apply = { label = "RAW" } }

-- Band Button Labels
rule { tags = {"id.band-160m"}, apply = { label = "160m" } }
rule { tags = {"id.band-80m"},  apply = { label = "80m" } }
rule { tags = {"id.band-40m"},  apply = { label = "40m" } }
rule { tags = {"id.band-20m"},  apply = { label = "20m" } }
rule { tags = {"id.band-15m"},  apply = { label = "15m" } }
rule { tags = {"id.band-10m"},  apply = { label = "10m" } }

-- RX Toggle
rule { tags = {"id.rx-toggle"}, apply = { label = "RX OFF" } }
rule { tags = {"id.rx-toggle", "state.Active"}, apply = { label = "RX ON" } }

-- Active Tags Debug Panel
rule {
    id = "id-active-tags",
    tags = {"id.active-tags"},
    apply = {
        parent = "active-tags",
        order = 10,
        title = "ACTIVE TAGS",
    }
}

rule {
    id = "id-active-tags-title",
    tags = {"id.active-tags-title"},
    apply = {
        text = "ACTIVE TAGS",
        foreground = "#80b3ff",
    }
}

print("[default.lua] Configuration loaded")
