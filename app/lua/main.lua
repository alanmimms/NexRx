--[[
  NexRx Application - Main Lua Entry Point

  This file is loaded by the C++ host and provides:
  - init()   : Called once at startup
  - update() : Called each frame with delta time
  - draw()   : Called each frame for rendering
]]

-- Add lua directory to package path for requires
local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path

-- Load UI module
local ui = require("ui.widgets")
local theme = require("ui.theme")

-- Global state
local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0

-- Application state
local rxActive = false
local frequency = 14.200  -- MHz
local volume = 0.8
local agcEnabled = true
local nrEnabled = false
local nbEnabled = false
local selectedMode = "USB"

-- Helper: Parse hex color to RGB (0-1 range)
local function hexToRgb(hex)
    hex = hex:gsub("#", "")
    local r = tonumber(hex:sub(1, 2), 16) / 255
    local g = tonumber(hex:sub(3, 4), 16) / 255
    local b = tonumber(hex:sub(5, 6), 16) / 255
    return r, g, b
end

-- Helper: Clamp value
local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

-- Get band name from frequency
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

-- Called once at startup
function init()
    print("[Lua] init() called")

    -- Set background color from SetBox
    local bg = setbox.getString("background", "#1a1a2e")
    local r, g, b = hexToRgb(bg)
    setClearColor(r, g, b)

    print("[Lua] NexRx UI initialized with immediate-mode widgets")
end

-- Called each frame
function update(dt)
    frameCount = frameCount + 1

    -- FPS calculation
    fpsAccum = fpsAccum + dt
    fpsFrames = fpsFrames + 1
    if fpsAccum >= 1.0 then
        fps = fpsFrames / fpsAccum
        fpsAccum = 0
        fpsFrames = 0
    end

    -- Mouse wheel for fine tuning frequency
    local wheel = getMouseWheel()
    if wheel ~= 0 then
        frequency = clamp(frequency + wheel * 0.01, 1.0, 30.0)
    end
end

-- Called each frame for rendering
function draw()
    local winW, winH = getWindowSize()
    local lineH = getLineHeight()
    local mouseX, mouseY = getMousePos()

    -- Begin UI frame
    ui.beginFrame()

    -- Main control panel
    local px, py, pw, ph = 30, 30, 300, 380
    local cx, cy, cw, ch = ui.panelWithTitle(px, py, pw, ph, "NexRx Control Panel")

    local y = cy

    -- RX toggle button
    local rxLabel = rxActive and "RX Active" or "RX Off"
    local rxTags = rxActive and {"Primary"} or {"Secondary"}
    rxActive = ui.toggle("rx_toggle", rxLabel, cx, y, 140, 36, rxActive, rxTags)
    y = y + 48

    -- Frequency display
    ui.label(cx, y, "Frequency:", {"Muted"})
    local freqStr = string.format("%.3f MHz", frequency)
    drawText(cx + 80, y, freqStr, 0.2, 0.9, 0.4, 1.0)
    y = y + 24

    -- Frequency slider
    frequency = ui.slider("freq_slider", cx, y, cw, 1.0, 30.0, frequency)
    y = y + 30

    -- Band indicator
    ui.label(cx, y, "Band: " .. getBand(frequency), {"Accent"})
    y = y + 28

    ui.separator(cx, y, cw)
    y = y + 12

    -- Volume control
    ui.label(cx, y, "Volume:")
    y = y + 22
    volume = ui.slider("volume_slider", cx, y, cw, 0, 1, volume)
    local volPct = string.format("%d%%", math.floor(volume * 100))
    drawText(cx + cw + 10, y - 6, volPct, 0.6, 0.6, 0.65, 1.0)
    y = y + 30

    ui.separator(cx, y, cw)
    y = y + 12

    -- DSP options
    ui.label(cx, y, "DSP Options:", {"Title"})
    y = y + 26

    agcEnabled = ui.checkbox("agc_checkbox", "AGC", cx, y, agcEnabled)
    y = y + 26

    nrEnabled = ui.checkbox("nr_checkbox", "Noise Reduction", cx, y, nrEnabled)
    y = y + 26

    nbEnabled = ui.checkbox("nb_checkbox", "Noise Blanker", cx, y, nbEnabled)
    y = y + 30

    ui.separator(cx, y, cw)
    y = y + 12

    -- Mode buttons (horizontal layout)
    ui.label(cx, y, "Mode:")
    y = y + 26
    local modes = {"USB", "LSB", "CW", "AM"}
    local btnW = (cw - 12) / 4
    for i, mode in ipairs(modes) do
        local bx = cx + (i - 1) * (btnW + 4)
        local isSelected = selectedMode == mode
        local tags = isSelected and {"Active"} or {}
        if ui.button("mode_" .. mode, mode, bx, y, btnW, 28, tags) then
            selectedMode = mode
            print("[Lua] Mode changed to: " .. mode)
        end
    end

    -- S-Meter panel
    local sx, sy, sw, sh = 30, 430, 300, 80
    ui.panel(sx, sy, sw, sh)
    ui.label(sx + 12, sy + 8, "S-Meter:")

    -- S-Meter bar background
    drawRoundedRect(sx + 12, sy + 32, sw - 24, 24, 4, 0.15, 0.15, 0.2, 1.0)

    -- Fake S-meter level (animated)
    local sLevel = math.abs(math.sin(frameCount * 0.02)) * 0.7 + 0.2
    for i = 0, 9 do
        local barX = sx + 16 + i * 26
        local barH = 16
        local intensity = (i + 1) / 10
        if intensity <= sLevel then
            local r, g, b = 0.2, 0.8, 0.3
            if i >= 7 then r, g, b = 0.9, 0.3, 0.2 end
            if i >= 5 and i < 7 then r, g, b = 0.9, 0.7, 0.2 end
            drawRect(barX, sy + 36, 22, barH, r, g, b, 1.0)
        else
            drawRect(barX, sy + 36, 22, barH, 0.25, 0.25, 0.3, 1.0)
        end
    end

    -- S-level text
    local sNum = math.floor(sLevel * 9) + 1
    local sText = sNum <= 9 and ("S" .. sNum) or ("S9+" .. ((sNum - 9) * 10))
    drawText(sx + sw - 50, sy + 38, sText, 0.9, 0.9, 0.95, 1.0)

    -- Instructions panel
    local ix, iy, iw, ih = 350, 30, 220, 120
    ui.panelWithTitle(ix, iy, iw, ih, "Controls")
    ui.label(ix + 12, iy + 40, "Click toggle for RX", {"Muted"})
    ui.label(ix + 12, iy + 58, "Drag sliders to adjust", {"Muted"})
    ui.label(ix + 12, iy + 76, "Scroll wheel for freq", {"Muted"})
    ui.label(ix + 12, iy + 94, "ESC to quit", {"Muted"})

    -- End UI frame
    ui.endFrame()

    -- Status bar at bottom
    drawRect(0, winH - 28, winW, 28, 0.08, 0.08, 0.1, 1.0)
    drawLine(0, winH - 28, winW, winH - 28, 0.3, 0.3, 0.35, 1.0, 1.0)

    local statusText = string.format("FPS: %.0f | Mouse: %d, %d | Frame: %d",
                                      fps, mouseX, mouseY, frameCount)
    drawText(10, winH - 22, statusText, 0.6, 0.6, 0.65, 1.0)

    -- NexRx branding
    local brand = "NexRx v0.1"
    local brandW = measureText(brand)
    drawText(winW - brandW - 10, winH - 22, brand, 0.4, 0.5, 0.7, 1.0)
end

print("[Lua] main.lua loaded")
