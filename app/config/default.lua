--[[
  NexRx Default Configuration - Exploded Hierarchy Edition
]]

-- =============================================================================
-- Global Defaults
-- =============================================================================

rule {
    id = "global-defaults",
    tags = {},
    priority = -100,
    apply = {
        ["rx.VFO.active"] = "A",
        ["rx.VFO.A"] = 14.2e6,
        ["rx.VFO.B"] = 7.1e6,
        ["rx.selectedMode"] = "USB",
        ["rx.selectedBand"] = "20m",
        ["rx.active"] = true,
        ["rx.volume.DB"] = -20,
        ["rx.volume.muted"] = false,
        ["rx.testToneEnabled"] = false,
        ["rx.AGC.mode"] = 0,
        ["rx.RF.gainDB"] = 30,
        ["rx.RF.attenuationDB"] = 0,
        ["filters.hpfBypass"] = false,
        ["filters.bpfIndex"] = 0,
        
        windowWidth = 1900,
        windowHeight = 850,
        fontSize = 16,
        fontPaths = { "fonts/DejaVuSans.ttf" },
        ["locale.thousandsSeparator"] = ",",

        stick = "LT",
        prefW = 0, prefH = 0,
        minW = 0, minH = 0, maxW = 10000, maxH = 10000,
        flexW = 0, flexH = 0,
        spacing = 4,
        padding = 12,
        background = "#1a1a2e",
        foreground = "#e2e8f0",
        borderWidth = 1,
    }
}

-- =============================================================================
-- Root Containers
-- =============================================================================

rule { id = "top-bar", tags = {"widget.TopBar"}, apply = { prefH = 32, stick = "TLR" } }

rule { id = "left-sidebar", tags = {"widget.Sidebar", "widget.LeftSidebar"}, apply = { minW = 280, prefW = 280, stick = "TLB", background = "#0f172a" } }

rule { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"}, apply = { minW = 300, prefW = 300, stick = "TRB", background = "#0f172a" } }

rule { id = "center-area", tags = {"widget.CenterArea"}, apply = { stick = "TLBR", flexW = 1.0 } }

-- =============================================================================
-- Left Sidebar Children
-- =============================================================================

rule { id = "id-rx-freq", tags = {"id.id-rx-freq", "widget.FrequencyDisplay"}, apply = { prefH = 40, stick = "TLR" } }

rule { id = "id-rx-r1k", tags = {"id.id-rx-r1k", "widget.Button"}, apply = { prefH = 24, flexW = 1, stick = "TLRB" } }
rule { id = "id-rx-r100", tags = {"id.id-rx-r100", "widget.Button"}, apply = { prefH = 24, flexW = 1, stick = "TLRB" } }

rule { id = "id-rx-slider", tags = {"id.id-rx-slider", "widget.Slider"}, apply = { prefH = 20, stick = "TLR" } }

-- Mode buttons
for _, m in ipairs({"LSB", "USB", "AM", "CW", "FM"}) do
    rule { id = "id-rx-mode-" .. m, tags = {"id.id-rx-mode-" .. m, "widget.Button"}, apply = { prefH = 26, flexW = 1, stick = "TLRB", label = m } }
end

-- Band buttons
for _, b in ipairs({"160m","80m","40m","20m","15m","10m"}) do
    rule { id = "id-rx-band-" .. b, tags = {"id.id-rx-band-" .. b, "widget.Button"}, apply = { prefH = 26, flexW = 1, stick = "TLRB", label = b } }
end

rule { id = "id-rx-vol", tags = {"id.id-rx-vol", "widget.Slider"}, apply = { prefH = 20, stick = "TLR" } }

rule { id = "id-rx-gain", tags = {"id.id-rx-gain", "widget.Slider"}, apply = { prefH = 20, stick = "TLR" } }

-- =============================================================================
-- Right Sidebar Children
-- =============================================================================

rule { id = "id-smeter", tags = {"id.id-smeter", "widget.SMeter"}, apply = { prefH = 66, stick = "TLR" } }

rule { id = "id-cal", tags = {"id.id-cal", "widget.Button"}, apply = { prefH = 30, label = "I/Q BAL CAL", stick = "TLR" } }

rule { id = "id-isg", tags = {"id.id-isg", "widget.ISGFrame"}, apply = { prefH = 240, background = "#00000000", stick = "TLR" } }

rule { id = "id-agc", tags = {"id.id-agc", "widget.AGCFrame"}, apply = { prefH = 80, background = "#00000000", stick = "TLR" } }

rule { id = "id-audio", tags = {"id.id-audio", "widget.AudioUtilsFrame"}, apply = { prefH = 100, background = "#00000000", stick = "TLR" } }

-- =============================================================================
-- Center Area Children
-- =============================================================================

rule { id = "id-spec", tags = {"id.id-spec", "widget.Spectrum"}, apply = { stick = "TLBR", flexH = 1.0, minH = 150 } }

rule { id = "id-wf", tags = {"id.id-wf", "widget.Waterfall"}, apply = { stick = "TLBR", flexH = 1.0, minH = 200 } }

-- =============================================================================
-- The River Workspace
-- =============================================================================

rule { id = "river-view-style", tags = {"widget.RiverView"}, apply = { background = "#050811", border = "#1e293b", stick = "TLBR", flexW = 1.0, flexH = 1.0 } }
rule { id = "gutter-left-style", tags = {"widget.GutterLeft"}, apply = { background = "#070c16", border = "#131f33", prefW = 76, stick = "TLB", flexH = 1.0 } }
rule { id = "live-trace-style", tags = {"widget.LiveTrace"}, apply = { background = "#070c16", border = "#131f33", prefW = 90, stick = "TRB", flexH = 1.0 } }
rule { id = "frequency-ruler-style", tags = {"widget.FrequencyRuler"}, apply = { background = "#0c101c", border = "#1e293b", prefW = 60, stick = "TRB", flexH = 1.0 } }

-- =============================================================================
-- DecoderTrack Mode Ribbons (Tag-Driven Customization)
-- =============================================================================

rule { id = "track-cw", tags = {"widget.DecoderTrack", "mode.CW"}, apply = { ribbonColor = "#3b82f6", textColor = "#93c5fd" } }
rule { id = "track-ft8", tags = {"widget.DecoderTrack", "mode.FT8"}, apply = { ribbonColor = "#f59e0b", textColor = "#fde68a" } }
rule { id = "track-js8", tags = {"widget.DecoderTrack", "mode.JS8"}, apply = { ribbonColor = "#f97316", textColor = "#fed7aa" } }
rule { id = "track-rtty", tags = {"widget.DecoderTrack", "mode.RTTY"}, apply = { ribbonColor = "#a855f7", textColor = "#e9d5ff" } }
rule { id = "track-usb", tags = {"widget.DecoderTrack", "mode.USB"}, apply = { ribbonColor = "#10b981", textColor = "#a7f3d0" } }
rule { id = "track-lsb", tags = {"widget.DecoderTrack", "mode.LSB"}, apply = { ribbonColor = "#06b6d4", textColor = "#cffafe" } }
rule { id = "track-am", tags = {"widget.DecoderTrack", "mode.AM"}, apply = { ribbonColor = "#ec4899", textColor = "#fbcfe8" } }
rule { id = "track-fm", tags = {"widget.DecoderTrack", "mode.FM"}, apply = { ribbonColor = "#84cc16", textColor = "#d9f99d" } }

-- =============================================================================
-- SignalBox Mode Styling (Tag-Driven Customization)
-- =============================================================================

rule { id = "sigbox-cw", tags = {"widget.SignalBox", "mode.CW"}, apply = { background = "#3b82f6", border = "#60a5fa" } }
rule { id = "sigbox-ft8", tags = {"widget.SignalBox", "mode.FT8"}, apply = { background = "#f59e0b", border = "#fbbf24" } }
rule { id = "sigbox-js8", tags = {"widget.SignalBox", "mode.JS8"}, apply = { background = "#f97316", border = "#fb923c" } }
rule { id = "sigbox-rtty", tags = {"widget.SignalBox", "mode.RTTY"}, apply = { background = "#a855f7", border = "#c084fc" } }
rule { id = "sigbox-usb", tags = {"widget.SignalBox", "mode.USB"}, apply = { background = "#10b981", border = "#34d399" } }
rule { id = "sigbox-lsb", tags = {"widget.SignalBox", "mode.LSB"}, apply = { background = "#06b6d4", border = "#22d3ee" } }
rule { id = "sigbox-am", tags = {"widget.SignalBox", "mode.AM"}, apply = { background = "#ec4899", border = "#f472b6" } }
rule { id = "sigbox-fm", tags = {"widget.SignalBox", "mode.FM"}, apply = { background = "#84cc16", border = "#a3e635" } }

print("[default.lua] Configuration loaded")
