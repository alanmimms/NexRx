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

-- VFO state
local vfoA = 14.200
local vfoB = 7.050
local activeVFO = "A"

-- Filter state
local filterWidth = 2400
local filterShift = 0

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
    print("[Lua] NexRx UI initialized with layout system")
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
        volume = ui.slider("volume", lx + 40, ly, w - 70, 0, 1, volume)
        local volPct = string.format("%d%%", math.floor(volume * 100))
        drawText(lx + w - 60, ly - 2, volPct, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Sql:")
        squelch = ui.slider("squelch", lx + 40, ly, w - 70, 0, 1, squelch)
        layout.newLine(24)
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
    -- CENTER - Main display area (waterfall placeholder)
    -- =========================================
    do
        local x, y, w, h = layout.getRect()
        layout.pad(8)
        local cx, cy, cw, ch = layout.getRect()

        -- Waterfall placeholder
        ui.panel(cx, cy, cw, ch)
        layout.pad(8)

        local px, py, pw, ph = layout.getRect()

        -- Title
        drawText(px, py, "Spectrum / Waterfall Display", 0.7, 0.7, 0.75, 1.0)

        -- Placeholder visualization
        local vizY = py + 30
        local vizH = ph - 40
        drawRoundedRect(px, vizY, pw, vizH, 4, 0.08, 0.08, 0.1, 1.0)

        -- Fake spectrum
        local specH = vizH * 0.4
        for i = 0, pw - 1, 2 do
            local noise = math.sin(i * 0.05 + frameCount * 0.1) * 0.3
            local signal = math.exp(-((i - pw/2)^2) / 2000) * 0.6
            local level = math.abs(noise + signal + math.random() * 0.1)
            local barH = level * specH
            local r, g, b = 0.2, 0.6, 0.3
            if level > 0.5 then r, g, b = 0.9, 0.7, 0.2 end
            if level > 0.7 then r, g, b = 0.9, 0.3, 0.2 end
            drawRect(px + i, vizY + specH - barH, 2, barH, r, g, b, 0.8)
        end

        -- Waterfall gradient placeholder
        local wfY = vizY + specH + 4
        local wfH = vizH - specH - 8
        for row = 0, wfH - 1, 4 do
            local rowY = wfY + row
            for col = 0, pw - 1, 4 do
                local noise = math.sin(col * 0.03 + (frameCount - row) * 0.05) * 0.3
                local signal = math.exp(-((col - pw/2)^2) / 3000) * 0.5
                local level = math.abs(noise + signal)
                local r = level * 0.8
                local g = level * 0.4
                local b = 0.2 + level * 0.3
                drawRect(px + col, rowY, 4, 4, r, g, b, 1.0)
            end
        end

        -- Center frequency marker
        local centerX = px + pw / 2
        drawLine(centerX, vizY, centerX, vizY + vizH, 0.9, 0.3, 0.3, 0.5, 1.0)
    end

    layout.finish()
    ui.endFrame()
end

print("[Lua] main.lua loaded")
