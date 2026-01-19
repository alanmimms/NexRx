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
        lmsMu = 0.001,                  -- LMS adaptive filter learning rate
        bfoOffset = 700,                -- Hz (CW sidetone)

        -- =================================================================
        -- Hardware Defaults
        -- =================================================================
        qsdOffsetK = 12.0,              -- kHz
        rfAttenDb = 0,                  -- dB (0-45 in 3 dB steps)

        -- =================================================================
        -- Display Defaults
        -- =================================================================
        waterfallBins = 512,
        waterfallRows = 256,
        colormap = "viridis",
        wfMinDb = -120,
        wfMaxDb = -40,
        spectrumEmaAlpha = 0.3,         -- Spectrum smoothing

        -- =================================================================
        -- Window Defaults
        -- =================================================================
        windowWidth = 1280,
        windowHeight = 850,
        fontSize = 16,

        -- =================================================================
        -- Font Search Paths (platform-specific, tried in order)
        -- =================================================================
        fontPaths = {
            -- Windows
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/tahoma.ttf",
            -- macOS
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/Library/Fonts/Arial.ttf",
            -- Linux
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            -- Fallback (bundled with app)
            "fonts/DejaVuSans.ttf",
        },

        -- =================================================================
        -- Connection Defaults (hw abstraction layer)
        -- =================================================================
        hwHost = "127.0.0.1",           -- Hardware/twin server address
        hwControlPort = 5000,           -- TCP control port
        hwStreamPort = 5001,            -- UDP stream port
        hwAutoConnect = true,           -- Auto-connect on startup

        -- Legacy names (for compatibility during transition)
        twinHost = "127.0.0.1",
        twinControlPort = 5000,
        twinStreamPort = 5001,
        twinAutoConnect = true,

        -- =================================================================
        -- Recording Defaults
        -- =================================================================
        recordingPath = "/tmp/nexrx_audio.wav",

        -- =================================================================
        -- Audio Defaults
        -- =================================================================
        rxVolume = 0.8,
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
    }
}

-- =============================================================================
-- UI Component Styles
-- =============================================================================

-- Button base
rule {
    id = "button-base",
    tags = {"Button"},
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
    tags = {"Button", "Primary"},
    apply = {
        background = "#2563eb",
        fontWeight = "bold",
    }
}

rule {
    id = "button-secondary",
    tags = {"Button", "Secondary"},
    apply = {
        background = "#64748b",
    }
}

rule {
    id = "button-danger",
    tags = {"Button", "Danger"},
    apply = {
        background = "#dc2626",
    }
}

-- Button state styles (tag-based, used by immediate-mode UI)
rule {
    id = "button-hovered",
    tags = {"Button", "Hovered"},
    priority = 10,
    apply = {
        background = "#60a5fa",
    }
}

rule {
    id = "button-pressed",
    tags = {"Button", "Pressed"},
    priority = 11,
    apply = {
        background = "#1d4ed8",
    }
}

rule {
    id = "button-active",
    tags = {"Button", "Active"},
    priority = 5,
    apply = {
        background = "#2563eb",
        border = "#60a5fa",
    }
}

rule {
    id = "button-disabled",
    tags = {"Button", "Disabled"},
    priority = 20,
    apply = {
        background = "#4a5568",
        foreground = "#a0aec0",
    }
}

-- Panel styles
rule {
    id = "panel-base",
    tags = {"Panel"},
    apply = {
        background = "#1e293b",
        borderRadius = 8,
        padding = 12,
    }
}

rule {
    id = "panel-sidebar",
    tags = {"Panel", "Sidebar"},
    apply = {
        background = "#0f172a",
        borderRadius = 0,
        width = 280,
    }
}

-- Slider styles
rule {
    id = "slider-base",
    tags = {"Slider"},
    apply = {
        background = "#2d3748",
        accent = "#3b82f6",
        border = "#4a5568",
    }
}

rule {
    id = "slider-hovered",
    tags = {"Slider", "Hovered"},
    priority = 10,
    apply = {
        accent = "#60a5fa",
    }
}

rule {
    id = "slider-active",
    tags = {"Slider", "Active"},
    priority = 11,
    apply = {
        accent = "#2563eb",
    }
}

-- Checkbox styles
rule {
    id = "checkbox-base",
    tags = {"Checkbox"},
    apply = {
        background = "#2d3748",
        foreground = "#e2e8f0",
        border = "#4a5568",
        accent = "#3b82f6",
    }
}

rule {
    id = "checkbox-checked",
    tags = {"Checkbox", "Checked"},
    priority = 5,
    apply = {
        background = "#3b82f6",
        border = "#60a5fa",
    }
}

rule {
    id = "checkbox-hovered",
    tags = {"Checkbox", "Hovered"},
    priority = 10,
    apply = {
        border = "#60a5fa",
    }
}

-- Input field styles
rule {
    id = "input-base",
    tags = {"Input"},
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
    tags = {"Input", "Hovered"},
    priority = 10,
    apply = {
        border = "#60a5fa",
    }
}

rule {
    id = "input-focused",
    tags = {"Input", "Focused"},
    priority = 11,
    apply = {
        border = "#3b82f6",
        borderWidth = 2,
    }
}

-- Label styles
rule {
    id = "label-base",
    tags = {"Label"},
    apply = {
        foreground = "#e2e8f0",
    }
}

rule {
    id = "label-title",
    tags = {"Label", "Title"},
    apply = {
        foreground = "#f1f5f9",
        fontSize = 16,
    }
}

rule {
    id = "label-muted",
    tags = {"Label", "Muted"},
    apply = {
        foreground = "#94a3b8",
    }
}

rule {
    id = "label-accent",
    tags = {"Label", "Accent"},
    apply = {
        foreground = "#60a5fa",
    }
}

-- Progress bar styles
rule {
    id = "progressbar-base",
    tags = {"ProgressBar"},
    apply = {
        background = "#2d3748",
        accent = "#3b82f6",
    }
}

-- Separator styles
rule {
    id = "separator-base",
    tags = {"Separator"},
    apply = {
        border = "#4a5568",
    }
}

-- =============================================================================
-- Radio Configuration Defaults
-- =============================================================================

-- Band defaults
rule {
    id = "radio-160m",
    tags = {"Radio", "160m"},
    apply = {
        frequency = 1.9e6,
        antenna = "wire",
    }
}

rule {
    id = "radio-80m",
    tags = {"Radio", "80m"},
    apply = {
        frequency = 3.5e6,
        antenna = "dipole",
    }
}

rule {
    id = "radio-40m",
    tags = {"Radio", "40m"},
    apply = {
        frequency = 7.0e6,
        antenna = "dipole",
    }
}

rule {
    id = "radio-20m",
    tags = {"Radio", "20m"},
    apply = {
        frequency = 14.0e6,
        antenna = "beam",
    }
}

rule {
    id = "radio-15m",
    tags = {"Radio", "15m"},
    apply = {
        frequency = 21.0e6,
        antenna = "beam",
    }
}

rule {
    id = "radio-10m",
    tags = {"Radio", "10m"},
    apply = {
        frequency = 28.0e6,
        antenna = "beam",
    }
}

-- Mode defaults
rule {
    id = "radio-cw",
    tags = {"Radio", "CW"},
    apply = {
        mode = "CW",
        filterWidth = 500,
        sidetoneFreq = 600,
        agcMode = "fast",
    }
}

rule {
    id = "radio-ssb",
    tags = {"Radio", "SSB"},
    apply = {
        mode = "USB",
        filterWidth = 2400,
        agcMode = "medium",
    }
}

rule {
    id = "radio-lsb",
    tags = {"Radio", "LSB"},
    apply = {
        mode = "LSB",
    }
}

rule {
    id = "radio-usb",
    tags = {"Radio", "USB"},
    apply = {
        mode = "USB",
    }
}

rule {
    id = "radio-am",
    tags = {"Radio", "AM"},
    apply = {
        mode = "AM",
        filterWidth = 6000,
        agcMode = "slow",
    }
}

-- Band + Mode combinations (more specific)
rule {
    id = "radio-40m-ssb",
    tags = {"Radio", "40m", "SSB"},
    apply = {
        frequency = 7.2e6,
        mode = "LSB",  -- 40m and below use LSB
    }
}

rule {
    id = "radio-20m-cw",
    tags = {"Radio", "20m", "CW"},
    apply = {
        frequency = 14.035e6,
    }
}

rule {
    id = "radio-20m-ssb",
    tags = {"Radio", "20m", "SSB"},
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
    tags = {"Waterfall"},
    apply = {
        colormap = "viridis",
        speed = 50,
        gain = 0,
        minDb = -120,
        maxDb = -40,
    }
}

rule {
    id = "waterfall-cw",
    tags = {"Waterfall", "CW"},
    apply = {
        colormap = "green_phosphor",
        span = 3000,
    }
}

rule {
    id = "waterfall-ssb",
    tags = {"Waterfall", "SSB"},
    apply = {
        span = 6000,
    }
}

-- =============================================================================
-- Contest Mode
-- =============================================================================

rule {
    id = "contest-base",
    tags = {"Contest"},
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
    tags = {"Contest", "CW"},
    apply = {
        filterWidth = 400,
        sidetoneFreq = 550,
    }
}

print("[default.lua] Configuration loaded")
