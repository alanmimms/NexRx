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

        stickLeft = false, stickRight = false, stickTop = false, stickBottom = false,
        width = 0, height = 0, 
        minWidth = 0, minHeight = 0, maxWidth = 9999, maxHeight = 9999,
        springX = 0, springY = 0,
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

rule { id = "top-bar", tags = {"widget.TopBar"}, apply = { height = 32, stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "left-sidebar", tags = {"widget.Sidebar", "widget.LeftSidebar"}, apply = { minWidth = 280, stickLeft = true, stickTop = true, stickBottom = true, background = "#0f172a" } }

rule { id = "right-sidebar", tags = {"widget.Sidebar", "widget.RightSidebar"}, apply = { minWidth = 300, stickRight = true, stickTop = true, stickBottom = true, background = "#0f172a" } }

rule { id = "center-area", tags = {"widget.CenterArea"}, apply = { stickTop = true, stickBottom = true, stickLeft = true, stickRight = true, springX = 1.0 } }

-- =============================================================================
-- Left Sidebar Children (Exploded)
-- =============================================================================

rule { id = "id-rx-freq", tags = {"id.id-rx-freq", "widget.FrequencyDisplay"}, apply = { parent = "left-sidebar", order = 1, height = 36, stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "id-rx-r1k", tags = {"id.id-rx-r1k", "widget.Button"}, apply = { parent = "left-sidebar", order = 2, height = 24, group = "rx-round", springX = 1, stickTop = true, stickLeft = true, stickRight = true } }
rule { id = "id-rx-r100", tags = {"id.id-rx-r100", "widget.Button"}, apply = { parent = "left-sidebar", order = 2, height = 24, group = "rx-round", springX = 1, stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "id-rx-slider", tags = {"id.id-rx-slider", "widget.Slider"}, apply = { parent = "left-sidebar", order = 3, height = 32, stickTop = true, stickLeft = true, stickRight = true } }

-- Mode buttons
for i, m in ipairs({"LSB", "USB", "AM", "CW", "FM"}) do
    rule { id = "id-rx-mode-" .. m, tags = {"id.id-rx-mode-" .. m, "widget.Button"}, apply = { parent = "left-sidebar", order = 4, height = 26, group = "rx-modes", springX = 1, stickTop = true, stickLeft = true, stickRight = true, label = m } }
end

-- Band buttons
for i, b in ipairs({"160m","80m","40m","20m","15m","10m"}) do
    rule { id = "id-rx-band-" .. b, tags = {"id.id-rx-band-" .. b, "widget.Button"}, apply = { parent = "left-sidebar", order = 5, height = 26, group = "rx-bands", springX = 1, stickTop = true, stickLeft = true, stickRight = true, label = b } }
end

rule { id = "id-rx-vol", tags = {"id.id-rx-vol", "widget.Slider"}, apply = { parent = "left-sidebar", order = 7, height = 32, label = "Volume", stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "id-rx-gain", tags = {"id.id-rx-gain", "widget.Slider"}, apply = { parent = "left-sidebar", order = 9, height = 32, label = "RF Gain", stickTop = true, stickLeft = true, stickRight = true } }

-- =============================================================================
-- Right Sidebar Children
-- =============================================================================

rule { id = "id-smeter", tags = {"id.id-smeter", "widget.SMeter"}, apply = { parent = "right-sidebar", order = 1, height = 70, stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "id-cal", tags = {"id.id-cal", "widget.Button"}, apply = { parent = "right-sidebar", order = 5, height = 30, label = "I/Q BAL CAL", stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "id-isg", tags = {"id.id-isg", "widget.ISGFrame"}, apply = { parent = "right-sidebar", order = 10, height = 240, background = "#00000000", stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "id-agc", tags = {"id.id-agc", "widget.AGCFrame"}, apply = { parent = "right-sidebar", order = 30, height = 80, background = "#00000000", stickTop = true, stickLeft = true, stickRight = true } }

rule { id = "id-audio", tags = {"id.id-audio", "widget.AudioUtilsFrame"}, apply = { parent = "right-sidebar", order = 40, height = 100, background = "#00000000", stickTop = true, stickLeft = true, stickRight = true } }

-- =============================================================================
-- Center Area Children
-- =============================================================================

rule { id = "id-spec", tags = {"id.id-spec", "widget.Spectrum"}, apply = { parent = "center-area", order = 10, stickTop = true, stickLeft = true, stickRight = true, springY = 2.0 } }

rule { id = "id-wf", tags = {"id.id-wf", "widget.Waterfall"}, apply = { parent = "center-area", order = 20, stickTop = true, stickBottom = true, stickLeft = true, stickRight = true, springY = 1.0 } }

-- =============================================================================
-- Far Right Sidebar
-- =============================================================================

rule { id = "active-tags", tags = {"widget.Sidebar", "widget.ActiveTagsSidebar"}, apply = { minWidth = 250, stickRight = true, stickTop = true, stickBottom = true, background = "#08081a" } }

rule { id = "id-active-tags", tags = {"id.id-active-tags", "widget.DebugPanel"}, apply = { parent = "active-tags", order = 10, stickTop = true, stickBottom = true, stickLeft = true, stickRight = true } }

print("[default.lua] Exploded configuration loaded")
