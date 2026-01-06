--[[
  NexRx Application - Main Lua Entry Point

  Demonstrates the layout system with a realistic SDR receiver UI.
]]

-- Add lua directory to package path for requires
local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path

-- Load UI modules
local ui = require("ui.widgets")
local theme = require("ui.theme")
local layout = require("ui.layout")

-- Global state
local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0

-- Application state
local rxActive = false
local frequency = 14.200  -- MHz
local volume = 0.8
local squelch = 0.3
local agcEnabled = true
local nrEnabled = false
local nbEnabled = false
local selectedMode = "USB"
local selectedBand = "20m"
local audioMuted = false
local testToneEnabled = false
local testToneFreq = 440

-- VFO state
local vfoA = 14.200
local vfoB = 7.050
local activeVFO = "A"

-- Filter state
local filterWidth = 2400
local filterShift = 0

-- Waterfall state
local waterfallBins = 512
local waterfallRows = 256
local spectrumData = {}
local selectedColormap = "viridis"

-- Helper functions
local function hexToRgb(hex)
    hex = hex:gsub("#", "")
    local r = tonumber(hex:sub(1, 2), 16) / 255
    local g = tonumber(hex:sub(3, 4), 16) / 255
    local b = tonumber(hex:sub(5, 6), 16) / 255
    return r, g, b
end

local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

local function getBand(freq)
    if freq >= 1.8 and freq <= 2.0 then return "160m"
    elseif freq >= 3.5 and freq <= 4.0 then return "80m"
    elseif freq >= 7.0 and freq <= 7.3 then return "40m"
    elseif freq >= 14.0 and freq <= 14.35 then return "20m"
    elseif freq >= 21.0 and freq <= 21.45 then return "15m"
    elseif freq >= 28.0 and freq <= 29.7 then return "10m"
    else return "OOB"
    end
end

-- Initialize
function init()
    print("[Lua] init() called")
    local bg = setbox.getString("background", "#1a1a2e")
    local r, g, b = hexToRgb(bg)
    setClearColor(r, g, b)

    -- Initialize audio
    if audio.isInitialized() then
        audio.setVolume(volume)
        audio.start()
        print("[Lua] Audio started at " .. audio.getSampleRate() .. " Hz")
    else
        print("[Lua] Warning: Audio not initialized")
    end

    -- Initialize waterfall
    if waterfall.init(waterfallBins, waterfallRows) then
        waterfall.setRange(-120, -40)
        waterfall.setColormap(selectedColormap)
        print("[Lua] Waterfall initialized: " .. waterfallBins .. "x" .. waterfallRows)
    else
        print("[Lua] Warning: Waterfall init failed")
    end

    -- Initialize spectrum data buffer
    for i = 1, waterfallBins do
        spectrumData[i] = -100
    end

    print("[Lua] NexRx UI initialized with layout system")
end

-- Generate simulated spectrum data
local function generateSpectrum()
    local centerBin = waterfallBins / 2
    for i = 1, waterfallBins do
        -- Base noise floor
        local noise = -100 + math.random() * 10

        -- Add some simulated signals
        local bin = i - 1

        -- Main carrier at center
        local distFromCenter = math.abs(bin - centerBin)
        if distFromCenter < 5 then
            noise = noise + 40 * math.exp(-distFromCenter * distFromCenter / 4)
        end

        -- Some side signals
        local signal1 = centerBin - 80 + math.sin(frameCount * 0.01) * 20
        local dist1 = math.abs(bin - signal1)
        if dist1 < 8 then
            noise = noise + 25 * math.exp(-dist1 * dist1 / 8) * (0.7 + 0.3 * math.sin(frameCount * 0.05))
        end

        local signal2 = centerBin + 120
        local dist2 = math.abs(bin - signal2)
        if dist2 < 6 then
            noise = noise + 30 * math.exp(-dist2 * dist2 / 6)
        end

        -- Occasional burst
        if math.random() < 0.001 then
            noise = noise + 20
        end

        spectrumData[i] = clamp(noise, -120, -20)
    end
end

-- Update
function update(dt)
    frameCount = frameCount + 1

    fpsAccum = fpsAccum + dt
    fpsFrames = fpsFrames + 1
    if fpsAccum >= 1.0 then
        fps = fpsFrames / fpsAccum
        fpsAccum = 0
        fpsFrames = 0
    end

    -- Mouse wheel for fine tuning
    local wheel = getMouseWheel()
    if wheel ~= 0 then
        frequency = clamp(frequency + wheel * 0.001, 1.0, 30.0)
        if activeVFO == "A" then vfoA = frequency else vfoB = frequency end
    end

    -- Generate simulated spectrum and update waterfall
    generateSpectrum()
    if waterfall.isInitialized() then
        waterfall.addRow(spectrumData)
    end
end

-- Draw
function draw()
    local winW, winH = getWindowSize()
    local mouseX, mouseY = getMousePos()

    ui.beginFrame()
    layout.begin(0, 0, winW, winH)

    -- =========================================
    -- TOP BAR - Status and branding
    -- =========================================
    layout.dock("top", 32)
    do
        local x, y, w, h = layout.getRect()
        drawRect(x, y, w, h, 0.08, 0.08, 0.12, 1.0)
        drawLine(x, y + h, x + w, y + h, 0.2, 0.2, 0.25, 1.0, 1.0)

        -- Branding
        drawText(x + 10, y + 8, "NexRx SDR Receiver", 0.6, 0.7, 0.9, 1.0)

        -- Status
        local statusText = string.format("FPS: %.0f", fps)
        local statusW = measureText(statusText)
        drawText(x + w - statusW - 10, y + 8, statusText, 0.5, 0.5, 0.55, 1.0)
    end
    layout.endDock()

    -- =========================================
    -- BOTTOM BAR - Status info
    -- =========================================
    layout.dock("bottom", 28)
    do
        local x, y, w, h = layout.getRect()
        drawRect(x, y, w, h, 0.08, 0.08, 0.1, 1.0)
        drawLine(x, y, x + w, y, 0.2, 0.2, 0.25, 1.0, 1.0)

        local info = string.format("Band: %s | Mode: %s | Filter: %d Hz | Mouse: %d, %d",
                                   getBand(frequency), selectedMode, filterWidth, mouseX, mouseY)
        drawText(x + 10, y + 6, info, 0.5, 0.5, 0.55, 1.0)
    end
    layout.endDock()

    -- =========================================
    -- LEFT SIDEBAR - Controls
    -- =========================================
    layout.dock("left", 260)
    do
        local x, y, w, h = layout.getRect()
        ui.panel(x, y, w, h, {"Sidebar"})
        layout.pad(12)

        -- VFO Section
        local lx, ly = layout.getCursor()
        ui.label(lx, ly, "VFO", {"Title"})
        layout.newLine(20)

        layout.beginHorizontal()
        local vfoATags = activeVFO == "A" and {"Primary"} or {"Secondary"}
        local vfoBTags = activeVFO == "B" and {"Primary"} or {"Secondary"}
        local bx, by = layout.reserveSpace(60, 28)
        if ui.button("vfo_a", "VFO A", bx, by, 60, 28, vfoATags) then
            activeVFO = "A"
            frequency = vfoA
        end
        bx, by = layout.reserveSpace(60, 28)
        if ui.button("vfo_b", "VFO B", bx, by, 60, 28, vfoBTags) then
            activeVFO = "B"
            frequency = vfoB
        end
        bx, by = layout.reserveSpace(50, 28)
        if ui.button("vfo_swap", "A<>B", bx, by, 50, 28) then
            vfoA, vfoB = vfoB, vfoA
            frequency = activeVFO == "A" and vfoA or vfoB
        end
        layout.endHorizontal()

        layout.space(8)

        -- Frequency display
        local freqStr = string.format("%.6f", frequency)
        local freqW = measureText(freqStr)
        local fx, fy = layout.getCursor()
        drawRoundedRect(fx, fy, w - 24, 36, 4, 0.1, 0.1, 0.15, 1.0)
        drawText(fx + (w - 24 - freqW) / 2, fy + 10, freqStr, 0.2, 0.9, 0.4, 1.0)
        layout.newLine(40)

        -- Frequency slider
        local sx, sy = layout.getCursor()
        frequency = ui.slider("freq_slider", sx, sy, w - 24, 1.0, 30.0, frequency)
        if activeVFO == "A" then vfoA = frequency else vfoB = frequency end
        layout.newLine(20)

        layout.space(8)
        local sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(8)

        -- Mode Selection
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Mode", {"Title"})
        layout.newLine(24)

        layout.beginHorizontal(4)
        local modes = {"USB", "LSB", "CW", "AM"}
        for _, mode in ipairs(modes) do
            local mx, my = layout.reserveSpace(50, 26)
            local tags = selectedMode == mode and {"Active"} or {}
            if ui.button("mode_" .. mode, mode, mx, my, 50, 26, tags) then
                selectedMode = mode
            end
        end
        layout.endHorizontal()

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(8)

        -- Filter Controls
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Filter", {"Title"})
        layout.newLine(24)

        local lx, ly = layout.getCursor()
        ui.label(lx, ly, "Width:")
        filterWidth = ui.slider("filter_width", lx + 50, ly, w - 80, 100, 6000, filterWidth)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Shift:")
        filterShift = ui.slider("filter_shift", lx + 50, ly, w - 80, -2000, 2000, filterShift)
        layout.newLine(24)

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(8)

        -- DSP Options
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "DSP", {"Title"})
        layout.newLine(24)

        local cx, cy = layout.getCursor()
        agcEnabled = ui.checkbox("agc", "AGC", cx, cy, agcEnabled)
        layout.newLine(24)

        cx, cy = layout.getCursor()
        nrEnabled = ui.checkbox("nr", "Noise Reduction", cx, cy, nrEnabled)
        layout.newLine(24)

        cx, cy = layout.getCursor()
        nbEnabled = ui.checkbox("nb", "Noise Blanker", cx, cy, nbEnabled)
        layout.newLine(28)

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(8)

        -- Volume & Squelch
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Audio", {"Title"})
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Vol:")
        local newVolume = ui.slider("volume", lx + 40, ly, w - 70, 0, 1, volume)
        if newVolume ~= volume then
            volume = newVolume
            audio.setVolume(volume)
        end
        local volPct = string.format("%d%%", math.floor(volume * 100))
        drawText(lx + w - 60, ly - 2, volPct, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Sql:")
        squelch = ui.slider("squelch", lx + 40, ly, w - 70, 0, 1, squelch)
        layout.newLine(24)

        -- Mute checkbox
        cx, cy = layout.getCursor()
        local newMuted = ui.checkbox("mute", "Mute", cx, cy, audioMuted)
        if newMuted ~= audioMuted then
            audioMuted = newMuted
            audio.setMuted(audioMuted)
        end
        layout.newLine(24)

        -- Test tone button
        local toneTags = testToneEnabled and {"Primary"} or {"Secondary"}
        local toneLabel = testToneEnabled and "Tone ON" or "Test Tone"
        local tx, ty = layout.getCursor()
        if ui.button("test_tone", toneLabel, tx, ty, 90, 26, toneTags) then
            testToneEnabled = not testToneEnabled
            audio.setTestTone(testToneEnabled, testToneFreq)
            if testToneEnabled then
                print("[Lua] Test tone ON: " .. testToneFreq .. " Hz")
            else
                print("[Lua] Test tone OFF")
            end
        end
        layout.newLine(28)
    end
    layout.endDock()

    -- =========================================
    -- RIGHT SIDEBAR - S-Meter and Band
    -- =========================================
    layout.dock("right", 200)
    do
        local x, y, w, h = layout.getRect()
        ui.panel(x, y, w, h, {"Sidebar"})
        layout.pad(12)

        -- RX Toggle
        local rx, ry = layout.getCursor()
        local rxLabel = rxActive and "RX ON" or "RX OFF"
        local rxTags = rxActive and {"Primary"} or {"Danger"}
        rxActive = ui.toggle("rx_toggle", rxLabel, rx, ry, w - 24, 40, rxActive, rxTags)
        layout.newLine(52)

        local sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(12)

        -- S-Meter
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "S-Meter", {"Title"})
        layout.newLine(24)

        local mx, my = layout.getCursor()
        local mw = w - 24
        drawRoundedRect(mx, my, mw, 28, 4, 0.12, 0.12, 0.15, 1.0)

        -- Animated S-level
        local sLevel = math.abs(math.sin(frameCount * 0.02)) * 0.7 + 0.2
        local barCount = 10
        local barW = (mw - 8) / barCount - 2
        for i = 0, barCount - 1 do
            local barX = mx + 4 + i * (barW + 2)
            local intensity = (i + 1) / barCount
            if intensity <= sLevel then
                local r, g, b = 0.2, 0.8, 0.3
                if i >= 7 then r, g, b = 0.9, 0.3, 0.2 end
                if i >= 5 and i < 7 then r, g, b = 0.9, 0.7, 0.2 end
                drawRect(barX, my + 4, barW, 20, r, g, b, 1.0)
            else
                drawRect(barX, my + 4, barW, 20, 0.2, 0.2, 0.25, 1.0)
            end
        end
        layout.newLine(40)

        local sNum = math.floor(sLevel * 9) + 1
        local sText = sNum <= 9 and ("S" .. sNum) or ("S9+" .. ((sNum - 9) * 10))
        local sx, sy = layout.center(measureText(sText), 16)
        drawText(sx, layout.getCursor() - 8, sText, 0.9, 0.9, 0.95, 1.0)
        layout.newLine(16)

        sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(12)

        -- Band Buttons
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Band", {"Title"})
        layout.newLine(24)

        local bands = {"160m", "80m", "40m", "20m", "15m", "10m"}
        local bandFreqs = {1.9, 3.5, 7.0, 14.0, 21.0, 28.0}

        for i, band in ipairs(bands) do
            if i % 2 == 1 then
                layout.beginHorizontal(4)
            end

            local bx, by = layout.reserveSpace(80, 26)
            local tags = getBand(frequency) == band and {"Active"} or {}
            if ui.button("band_" .. band, band, bx, by, 80, 26, tags) then
                frequency = bandFreqs[i]
                if activeVFO == "A" then vfoA = frequency else vfoB = frequency end
            end

            if i % 2 == 0 or i == #bands then
                layout.endHorizontal()
            end
        end
    end
    layout.endDock()

    -- =========================================
    -- CENTER - Spectrum and Waterfall Display
    -- =========================================
    do
        local x, y, w, h = layout.getRect()
        layout.pad(8)
        local cx, cy, cw, ch = layout.getRect()

        -- Panel background
        ui.panel(cx, cy, cw, ch)
        layout.pad(8)

        local px, py, pw, ph = layout.getRect()

        -- Title bar with frequency info
        local freqStr = string.format("%.6f MHz", frequency)
        drawText(px, py, freqStr, 0.2, 0.9, 0.4, 1.0)

        -- Colormap selector (right side of title)
        local colormaps = {"viridis", "plasma", "inferno", "green", "blue"}
        local cmapX = px + pw - 280
        for i, cmap in ipairs(colormaps) do
            local btnX = cmapX + (i - 1) * 55
            local tags = selectedColormap == cmap and {"Active"} or {}
            if ui.button("cmap_" .. cmap, cmap, btnX, py - 2, 52, 20, tags) then
                selectedColormap = cmap
                waterfall.setColormap(cmap)
            end
        end

        -- Display area
        local vizY = py + 24
        local vizH = ph - 30

        -- Spectrum (top 35%)
        local specH = math.floor(vizH * 0.35)
        if waterfall.isInitialized() then
            waterfall.renderSpectrum(spectrumData, px, vizY, pw, specH)
        end

        -- Separator line
        drawLine(px, vizY + specH + 2, px + pw, vizY + specH + 2, 0.3, 0.3, 0.4, 1.0, 1.0)

        -- Waterfall (bottom 65%)
        local wfY = vizY + specH + 4
        local wfH = vizH - specH - 8
        if waterfall.isInitialized() then
            waterfall.render(px, wfY, pw, wfH)
        end

        -- Center frequency marker
        local centerX = px + pw / 2
        drawLine(centerX, vizY, centerX, vizY + vizH, 0.9, 0.3, 0.3, 0.5, 1.0)

        -- Frequency scale labels (simplified)
        local spanKHz = 100  -- Simulated span
        drawText(px + 5, vizY + vizH - 16, string.format("-%.0f kHz", spanKHz/2), 0.5, 0.5, 0.6, 1.0)
        drawText(px + pw - 55, vizY + vizH - 16, string.format("+%.0f kHz", spanKHz/2), 0.5, 0.5, 0.6, 1.0)
    end

    layout.finish()
    ui.endFrame()
end

print("[Lua] main.lua loaded")
