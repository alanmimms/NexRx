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
        ["rx.VFO.A"] = 7.1e6,
        ["rx.VFO.B"] = 14.2e6,
        ["rx.selectedMode"] = "LSB",
        ["rx.selectedBand"] = "40m",
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
-- Far Right Sidebar
-- =============================================================================

rule { id = "active-tags", tags = {"widget.Sidebar", "widget.ActiveTagsSidebar"}, apply = { minW = 180, prefW = 180, stick = "TRB", background = "#08081a" } }

rule { id = "id-active-tags", tags = {"id.id-active-tags", "widget.ActiveTags"}, apply = { stick = "TLBR", flexH = 1 } }

print("[default.lua] Configuration loaded")
