-- NexRx Mode Configuration via SetBox
--
-- This file defines radio operating modes using SetBox's tag-based system.
-- Tags represent operating context (band, mode, activity), and rules
-- define the configuration for each combination.
--
-- Resolution order: more specific (more tags) wins > higher priority > later declaration
--
-- Usage in main.lua:
--   setbox.loadFile("config/modes.lua")
--   setbox.onPropertyChange(function(name, value)
--       if config[name] ~= nil then config[name] = value end
--   end)
--   setbox.addTag("cw")
--   setbox.addTag("40m")

-- =============================================================================
-- Global defaults (no tags = lowest specificity, always matches)
-- =============================================================================
rule {
    filterBandwidth = 2400,
    filterShape = "pointed",
    agcSpeed = "medium",
    noiseReduction = false,
    notchFilter = false
}

-- =============================================================================
-- Mode-specific defaults
-- =============================================================================

-- SSB defaults
rule {
    tags = {"ssb"},
    filterBandwidth = 2400,
    filterShape = "pointed",
    agcSpeed = "medium"
}

-- CW defaults
rule {
    tags = {"cw"},
    filterBandwidth = 500,
    filterShape = "pointed",
    agcSpeed = "fast",
    cwSidetone = 700
}

-- AM defaults (for broadcast listening)
rule {
    tags = {"am"},
    filterBandwidth = 6000,
    filterShape = "flat",
    agcSpeed = "slow"
}

-- =============================================================================
-- Band-specific adjustments (more specific = overrides mode defaults)
-- =============================================================================

-- 80m CW - wider for ragchewing, QRN can be bad
rule {
    tags = {"cw", "80m"},
    filterBandwidth = 800,
    noiseReduction = true
}

-- 40m CW - moderate, good balance
rule {
    tags = {"cw", "40m"},
    filterBandwidth = 500
}

-- 20m CW - tighter for DX
rule {
    tags = {"cw", "20m"},
    filterBandwidth = 400
}

-- 80m SSB - wider for local nets
rule {
    tags = {"ssb", "80m"},
    filterBandwidth = 2700
}

-- =============================================================================
-- Activity-specific (even more specific)
-- =============================================================================

-- Contest CW - very tight filters
rule {
    tags = {"cw", "contest"},
    filterBandwidth = 250,
    agcSpeed = "fast"
}

-- Contest CW on 20m - your personal preference: extra tight
rule {
    tags = {"cw", "contest", "20m"},
    filterBandwidth = 200
}

-- DXing - need to hear weak ones
rule {
    tags = {"dx"},
    agcSpeed = "slow",
    noiseReduction = true
}

-- Pileup operation - fast AGC, tight filter
rule {
    tags = {"dx", "pileup"},
    agcSpeed = "fast",
    filterBandwidth = 300
}

-- =============================================================================
-- Special modes
-- =============================================================================

-- FT8/digital modes
rule {
    tags = {"digital"},
    filterBandwidth = 3000,
    filterShape = "flat",
    agcSpeed = "off",
    noiseReduction = false
}

-- RTTY
rule {
    tags = {"rtty"},
    filterBandwidth = 500,
    filterShape = "flat",
    notchFilter = true
}
