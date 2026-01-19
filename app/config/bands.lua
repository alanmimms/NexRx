--[[
    NexRx Band Definitions

    Defines amateur radio bands as SetBox rules. The bands.lua module uses
    these definitions to determine the current band based on frequency.
    When frequency changes, the band tag is automatically updated, which
    triggers re-evaluation of band-specific rules.

    Each band rule has:
    - tags = {"BandDef"} - Marks this as a band definition
    - bandName - The band name (becomes a SetBox tag when active)
    - bandStartHz - Lower edge in Hz
    - bandEndHz - Upper edge in Hz
    - bandDefaultFreq - Default tuning frequency in Hz

    Band-specific behavior rules can use tags like {"Radio", "40m", "SSB"}
    to activate only when on that band.
]]

-- =============================================================================
-- HF Bands
-- =============================================================================

rule {
    id = "band-def-160m",
    tags = {"BandDef"},
    apply = {
        bandName = "160m",
        bandStartHz = 1.8e6,
        bandEndHz = 2.0e6,
        bandDefaultFreq = 1.9e6,
    }
}

rule {
    id = "band-def-80m",
    tags = {"BandDef"},
    apply = {
        bandName = "80m",
        bandStartHz = 3.5e6,
        bandEndHz = 4.0e6,
        bandDefaultFreq = 3.75e6,
    }
}

rule {
    id = "band-def-60m",
    tags = {"BandDef"},
    apply = {
        bandName = "60m",
        bandStartHz = 5.3305e6,
        bandEndHz = 5.4065e6,
        bandDefaultFreq = 5.3665e6,
    }
}

rule {
    id = "band-def-40m",
    tags = {"BandDef"},
    apply = {
        bandName = "40m",
        bandStartHz = 7.0e6,
        bandEndHz = 7.3e6,
        bandDefaultFreq = 7.15e6,
    }
}

rule {
    id = "band-def-30m",
    tags = {"BandDef"},
    apply = {
        bandName = "30m",
        bandStartHz = 10.1e6,
        bandEndHz = 10.15e6,
        bandDefaultFreq = 10.125e6,
    }
}

rule {
    id = "band-def-20m",
    tags = {"BandDef"},
    apply = {
        bandName = "20m",
        bandStartHz = 14.0e6,
        bandEndHz = 14.35e6,
        bandDefaultFreq = 14.2e6,
    }
}

rule {
    id = "band-def-17m",
    tags = {"BandDef"},
    apply = {
        bandName = "17m",
        bandStartHz = 18.068e6,
        bandEndHz = 18.168e6,
        bandDefaultFreq = 18.1e6,
    }
}

rule {
    id = "band-def-15m",
    tags = {"BandDef"},
    apply = {
        bandName = "15m",
        bandStartHz = 21.0e6,
        bandEndHz = 21.45e6,
        bandDefaultFreq = 21.2e6,
    }
}

rule {
    id = "band-def-12m",
    tags = {"BandDef"},
    apply = {
        bandName = "12m",
        bandStartHz = 24.89e6,
        bandEndHz = 24.99e6,
        bandDefaultFreq = 24.93e6,
    }
}

rule {
    id = "band-def-10m",
    tags = {"BandDef"},
    apply = {
        bandName = "10m",
        bandStartHz = 28.0e6,
        bandEndHz = 29.7e6,
        bandDefaultFreq = 28.5e6,
    }
}

-- =============================================================================
-- VHF Bands
-- =============================================================================

rule {
    id = "band-def-6m",
    tags = {"BandDef"},
    apply = {
        bandName = "6m",
        bandStartHz = 50.0e6,
        bandEndHz = 54.0e6,
        bandDefaultFreq = 50.125e6,
    }
}

-- =============================================================================
-- Band-Specific Mode Rules
-- These activate when both band tag and mode tag are present
-- =============================================================================

-- 40m and below use LSB for SSB
rule {
    id = "band-40m-ssb-mode",
    tags = {"Radio", "40m", "SSB"},
    priority = 5,
    apply = {
        sideband = "LSB",
    }
}

rule {
    id = "band-80m-ssb-mode",
    tags = {"Radio", "80m", "SSB"},
    priority = 5,
    apply = {
        sideband = "LSB",
    }
}

rule {
    id = "band-160m-ssb-mode",
    tags = {"Radio", "160m", "SSB"},
    priority = 5,
    apply = {
        sideband = "LSB",
    }
}

-- 20m and above use USB for SSB
rule {
    id = "band-20m-ssb-mode",
    tags = {"Radio", "20m", "SSB"},
    priority = 5,
    apply = {
        sideband = "USB",
    }
}

rule {
    id = "band-15m-ssb-mode",
    tags = {"Radio", "15m", "SSB"},
    priority = 5,
    apply = {
        sideband = "USB",
    }
}

rule {
    id = "band-10m-ssb-mode",
    tags = {"Radio", "10m", "SSB"},
    priority = 5,
    apply = {
        sideband = "USB",
    }
}

-- =============================================================================
-- Band-Specific CW Frequencies (common calling frequencies)
-- =============================================================================

rule {
    id = "band-40m-cw-freq",
    tags = {"Radio", "40m", "CW"},
    priority = 5,
    apply = {
        cwCallingFreq = 7.03e6,
    }
}

rule {
    id = "band-20m-cw-freq",
    tags = {"Radio", "20m", "CW"},
    priority = 5,
    apply = {
        cwCallingFreq = 14.03e6,
    }
}

rule {
    id = "band-15m-cw-freq",
    tags = {"Radio", "15m", "CW"},
    priority = 5,
    apply = {
        cwCallingFreq = 21.03e6,
    }
}

rule {
    id = "band-10m-cw-freq",
    tags = {"Radio", "10m", "CW"},
    priority = 5,
    apply = {
        cwCallingFreq = 28.03e6,
    }
}

print("[bands.lua] Band definitions loaded")
