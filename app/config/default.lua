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
        defaultFrequency = 14.200e6,    -- Hz (20m band)
        defaultMode = "USB",
        defaultBand = "20m",
        rxActive = true,
        vfoA = 14.200e6,                -- Hz
        vfoB = 7.050e6,                 -- Hz
        activeVFO = "A",

        -- =================================================================
        -- DSP Defaults
        -- =================================================================
        squelch = 0.3,
        agcEnabled = true,
        nrEnabled = false,
        nbEnabled = false,
        lmsMu = 0.5,                    -- LMS adaptive filter learning rate
        bfoOffset = 700,                -- Hz (CW sidetone)
        
        -- Filter Defaults
        bandpassEnabled = false,
        bandpassCenter = 700,
        bandpassWidth = 500,
        notchEnabled = false,
        notchCenter = 0,
        notchWidth = 100,

        -- =================================================================
        -- Hardware Defaults
        -- =================================================================
        qsdOffsetK = 12.0,              -- kHz
        rfAttenDb = 0,                  -- dB (0-45 in 3 dB steps)

        -- Preselector
        preselL1 = false,
        preselC0 = false, preselC1 = false, preselC2 = false, preselC3 = false, preselC4 = false,
        preselC5 = false, preselC6 = false, preselC7 = false, preselC8 = false, preselC9 = false,
        preselC10 = false,

        -- =================================================================
        -- Display Defaults
        -- =================================================================
        wfBins = 512,
        wfRows = 256,
        wfColormap = "viridis",
        wfMinDb = -140,
        wfMaxDb = -40,
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
        volumeDb = -40,             -- dB, range -60 to 0
        muted = false,

        -- =================================================================
        -- UI Theme Colors
        -- =================================================================
        background = "#1a1a2e",
        foreground = "#e2e8f0",
        accent = "#3b82f6",

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
    }
}

rule {
    id = "button-pressed",
    tags = {"widget.Button", "state.Pressed"},
    priority = 11,
    apply = {
        background = "#1d4ed8",
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
        color_weak = "#22c55e",   -- Green (S1-S5)
        color_mid = "#eab308",    -- Yellow (S6-S9)
        color_strong = "#ef4444", -- Red (S9+)
        color_off = "#334155",    -- Dark blue-gray (inactive)
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

print("[default.lua] Configuration loaded")
