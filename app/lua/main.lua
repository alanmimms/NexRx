--[[
  NexRx Application - Main Lua Entry Point

  Demonstrates the layout system with a realistic SDR receiver UI.

  Configuration API:
  The 'config' global provides typed access to receiver settings.
  Example usage:
    config.volume = 0.5           -- Set volume (0-1)
    config.mode = 1               -- USB=0, LSB=1, CW=2, AM=3
    config.setMode(1)             -- Explicit setter
    print(config.qsdOffsetK)      -- Get QSD offset
    config.resetDefaults()        -- Invoke action
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
-- Note: volume, muted, testTone* now use config.* API (see header comment)
local rxActive = false
local frequency = 14.200  -- MHz
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

-- LMS adaptive filter
local lmsMu = 0.001  -- Learning rate for image rejection

-- QSD offset k (kHz) for image rejection
local qsdOffsetK = 12.0  -- Default 12 kHz

-- RF Attenuator (0-45 dB in 3 dB steps)
local rfAttenDb = 0  -- Default 0 dB (no attenuation)

-- Waterfall state
local waterfallBins = 512
local waterfallRows = 256
local spectrumData = {}
local selectedColormap = "viridis"
local wfMinDb = -120  -- Waterfall min dB (noise floor)
local wfMaxDb = -40   -- Waterfall max dB (strongest signals)

-- Twin connection state (can be modified before calling connectTwin())
local twinSettings = {
    host = "127.0.0.1",
    controlPort = 5000,
    streamPort = 5001,
}

-- Connect to twin with current settings
local function connectTwin()
    if twin.isConnected() then
        twin.disconnect()
    end

    print("[Lua] Connecting to twin at " .. twinSettings.host ..
          " (TCP:" .. twinSettings.controlPort .. ", UDP:" .. twinSettings.streamPort .. ")...")

    if twin.connect(twinSettings.host, twinSettings.controlPort, twinSettings.streamPort) then
        twinConnected = true
        print("[Lua] Connected to digital twin!")
        -- Send initial QSD offset k
        twin.setQsdOffset(qsdOffsetK)
        print("[Lua] Set QSD offset k = " .. qsdOffsetK .. " kHz")
        return true
    else
        twinConnected = false
        print("[Lua] Failed to connect to twin")
        return false
    end
end

-- Disconnect from twin
local function disconnectTwin()
    if twin.isConnected() then
        twin.disconnect()
        twinConnected = false
        print("[Lua] Disconnected from twin")
    end
end

-- Twin connection state
local twinConnected = false
local twinFramesReceived = 0
local useTwinSpectrum = true  -- Try to use twin data when available
local lastVfoFreq = 0  -- Track VFO changes for twin control

-- Frequency entry state
local freqEntryMode = false
local freqEntryText = ""
local freqEntryBlink = 0

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
        audio.setVolume(config.volume)  -- Use config API
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

    -- Load twin connection settings from config
    twinSettings.host = setbox.getString("twinHost", "127.0.0.1")
    twinSettings.controlPort = math.floor(setbox.getNumber("twinControlPort", 5000))
    twinSettings.streamPort = math.floor(setbox.getNumber("twinStreamPort", 5001))

    -- Try to connect to twin (digital twin simulation)
    local autoConnect = setbox.getBool("twinAutoConnect", true)
    if autoConnect then
        if not connectTwin() then
            print("[Lua] Twin not available - using simulated spectrum")
        end
    else
        twinConnected = false
        print("[Lua] Twin auto-connect disabled")
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

    -- Frequency entry mode handling
    freqEntryBlink = freqEntryBlink + dt
    if freqEntryBlink > 1.0 then freqEntryBlink = 0 end

    -- SDL Scancodes
    local SC_F = 9
    local SC_RETURN = 40
    local SC_ESCAPE = 41
    local SC_BACKSPACE = 42

    if freqEntryMode then
        -- Get text input from SDL (handles all typing correctly)
        local textIn = getTextInput()
        for i = 1, #textIn do
            local ch = textIn:sub(i, i)
            -- Only allow digits and decimal point
            if ch:match("[0-9]") or (ch == "." and not freqEntryText:find("%.")) then
                freqEntryText = freqEntryText .. ch
            end
        end

        -- Backspace
        if isKeyPressed(SC_BACKSPACE) and #freqEntryText > 0 then
            freqEntryText = freqEntryText:sub(1, -2)
        end
        -- Enter - apply frequency
        if isKeyPressed(SC_RETURN) then
            local newFreq = tonumber(freqEntryText)
            if newFreq and newFreq >= 0.1 and newFreq <= 30.0 then
                frequency = newFreq
                if activeVFO == "A" then vfoA = frequency else vfoB = frequency end
            end
            freqEntryMode = false
            freqEntryText = ""
        end
        -- Escape - cancel
        if isKeyPressed(SC_ESCAPE) then
            freqEntryMode = false
            freqEntryText = ""
        end
    else
        -- 'F' key to start frequency entry
        if isKeyPressed(SC_F) then
            freqEntryMode = true
            freqEntryText = ""
        end
    end

    -- Mouse wheel for tuning with modifier support
    -- Shift+scroll = 100 kHz steps
    -- Ctrl+scroll = 100 Hz steps
    -- Plain scroll = 1 kHz steps
    local wheel = getMouseWheel()
    if wheel ~= 0 and not freqEntryMode then
        local step
        if isShiftDown() then
            step = 0.1  -- 100 kHz
        elseif isCtrlDown() then
            step = 0.0001  -- 100 Hz
        else
            step = 0.001  -- 1 kHz default
        end
        frequency = clamp(frequency + wheel * step, 0.1, 30.0)
        if activeVFO == "A" then vfoA = frequency else vfoB = frequency end
    end

    -- ESC to quit (when not in frequency entry mode)
    if isKeyPressed(SC_ESCAPE) and not freqEntryMode then
        quit()
    end

    -- Get spectrum data from twin or generate simulated
    local gotTwinData = false
    if useTwinSpectrum and twin.isConnected() then
        local twinSpectrum = twin.getSpectrum()
        if #twinSpectrum > 0 then
            -- Resample twin spectrum to our bin count if needed
            if #twinSpectrum == waterfallBins then
                spectrumData = twinSpectrum
            else
                -- Simple resampling
                local ratio = #twinSpectrum / waterfallBins
                for i = 1, waterfallBins do
                    local srcIdx = math.floor((i - 1) * ratio) + 1
                    spectrumData[i] = twinSpectrum[srcIdx] or -100
                end
            end
            gotTwinData = true
            twinFramesReceived = twin.getFramesReceived()
        end
        twinConnected = true
    else
        twinConnected = twin.isConnected()
    end

    -- Fall back to simulated spectrum if no twin data
    if not gotTwinData then
        generateSpectrum()
    end

    -- Update waterfall
    if waterfall.isInitialized() then
        waterfall.addRow(spectrumData)
    end

    -- Send VFO changes to twin (frequency is in MHz, convert to Hz)
    local currentVfoHz = frequency * 1e6
    if currentVfoHz ~= lastVfoFreq then
        rx.setVfo(currentVfoHz)
        lastVfoFreq = currentVfoHz
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

        -- Twin connection status
        local twinStatus = twinConnected and "TWIN" or "SIM"
        local twinColor = twinConnected and {0.2, 0.9, 0.4} or {0.6, 0.6, 0.6}
        drawText(x + 200, y + 8, twinStatus, twinColor[1], twinColor[2], twinColor[3], 1.0)
        if twinConnected then
            local framesText = string.format(" (%d frames)", twinFramesReceived)
            drawText(x + 200 + measureText(twinStatus) + 4, y + 8, framesText, 0.5, 0.5, 0.55, 1.0)

            -- Audio stats for debugging (now includes drops and fill ratio)
            local audioWritten, audioRead, underruns, drops, fillRatio = rx.getAudioStats()
            local audioText = string.format("Buf: %.0f%%", fillRatio * 100)
            drawText(x + 400, y + 8, audioText, 0.5, 0.7, 0.5, 1.0)

            -- Drop rates (IQ and Audio)
            local iqDropRate = twin.getIqDropRate()
            local audioDropRate = rx.getAudioDropRate()
            local dropText = string.format("Drop: IQ=%.0f/s A=%.0f/s", iqDropRate, audioDropRate)
            -- Color red if drops > 0
            local dropR = (iqDropRate > 0 or audioDropRate > 0) and 1.0 or 0.5
            local dropG = (iqDropRate > 0 or audioDropRate > 0) and 0.3 or 0.7
            drawText(x + 500, y + 8, dropText, dropR, dropG, 0.5, 1.0)
        end

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

        local bpInfo = config.bandpassEnabled and string.format("BP: %.0f Hz", config.bandpassWidth) or "BP: Off"
        local info = string.format("Band: %s | Mode: %s | %s | Mouse: %d, %d",
                                   getBand(frequency), selectedMode, bpInfo, mouseX, mouseY)
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
                rx.setMode(mode)  -- Update demodulator mode
            end
        end
        layout.endHorizontal()

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(8)

        -- Baseband Filter (FIR)
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Baseband", {"Title"})
        layout.newLine(24)

        -- Bandpass enable
        local cx, cy = layout.getCursor()
        local newBpEn = ui.checkbox("bp_en", "Bandpass", cx, cy, config.bandpassEnabled)
        if newBpEn ~= config.bandpassEnabled then
            config.bandpassEnabled = newBpEn
        end
        layout.newLine(24)

        -- Bandpass center (only show if enabled)
        if config.bandpassEnabled then
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Center:")
            local newBpCenter = ui.slider("bp_center", lx + 55, ly, w - 85, -5000, 5000, config.bandpassCenter)
            if newBpCenter ~= config.bandpassCenter then
                config.bandpassCenter = newBpCenter
            end
            local bpCText = string.format("%.0f Hz", config.bandpassCenter)
            drawText(lx + w - 70, ly - 2, bpCText, 0.5, 0.5, 0.55, 1.0)
            layout.newLine(24)

            -- Bandpass width
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Width:")
            local newBpWidth = ui.slider("bp_width", lx + 55, ly, w - 85, 50, 4000, config.bandpassWidth)
            if newBpWidth ~= config.bandpassWidth then
                config.bandpassWidth = newBpWidth
            end
            local bpWText = string.format("%.0f Hz", config.bandpassWidth)
            drawText(lx + w - 70, ly - 2, bpWText, 0.5, 0.5, 0.55, 1.0)
            layout.newLine(24)
        end

        -- Notch enable
        cx, cy = layout.getCursor()
        local newNotchEn = ui.checkbox("notch_en", "Notch", cx, cy, config.notchEnabled)
        if newNotchEn ~= config.notchEnabled then
            config.notchEnabled = newNotchEn
        end
        layout.newLine(24)

        -- Notch center (only show if enabled)
        if config.notchEnabled then
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Center:")
            local newNotchCenter = ui.slider("notch_center", lx + 55, ly, w - 85, -10000, 10000, config.notchCenter)
            if newNotchCenter ~= config.notchCenter then
                config.notchCenter = newNotchCenter
            end
            local notchCText = string.format("%.0f Hz", config.notchCenter)
            drawText(lx + w - 70, ly - 2, notchCText, 0.5, 0.5, 0.55, 1.0)
            layout.newLine(24)

            -- Notch width
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Width:")
            local newNotchWidth = ui.slider("notch_width", lx + 55, ly, w - 85, 10, 500, config.notchWidth)
            if newNotchWidth ~= config.notchWidth then
                config.notchWidth = newNotchWidth
            end
            local notchWText = string.format("%.0f Hz", config.notchWidth)
            drawText(lx + w - 70, ly - 2, notchWText, 0.5, 0.5, 0.55, 1.0)
            layout.newLine(24)
        end

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

        -- QSD offset k for image rejection
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "QSD k:")
        local newK = ui.slider("qsd_k", lx + 50, ly, w - 80, 1, 24, qsdOffsetK)
        if newK ~= qsdOffsetK then
            qsdOffsetK = newK
            twin.setQsdOffset(qsdOffsetK)
        end
        local kText = string.format("%.1f kHz", qsdOffsetK)
        drawText(lx + w - 70, ly - 2, kText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        -- LMS mu (learning rate) for image rejection
        -- Logarithmic scale: slider 0-1 maps to mu 0.0001 to 0.1
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "LMS:")
        local function muToSlider(mu)
            -- Map 0.0001..0.1 to 0..1 (log scale)
            return (math.log(mu) - math.log(0.0001)) / (math.log(0.1) - math.log(0.0001))
        end
        local function sliderToMu(pos)
            -- Map 0..1 to 0.0001..0.1 (log scale)
            local logMu = math.log(0.0001) + pos * (math.log(0.1) - math.log(0.0001))
            return math.exp(logMu)
        end
        local muSliderPos = muToSlider(lmsMu)
        local newMuSliderPos = ui.slider("lms_mu", lx + 40, ly, w - 70, 0, 1, muSliderPos)
        if newMuSliderPos ~= muSliderPos then
            lmsMu = sliderToMu(newMuSliderPos)
            rx.setLmsMu(lmsMu)
        end
        local muText = string.format("%.4f", lmsMu)
        drawText(lx + w - 60, ly - 2, muText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        -- RF Attenuator (0-45 dB in 3 dB steps)
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Atten:")
        -- Slider goes 0-15 (steps), displayed as 0-45 dB
        local attenSteps = rfAttenDb / 3
        local newAttenSteps = ui.slider("rf_atten", lx + 50, ly, w - 80, 0, 15, attenSteps)
        local newAttenDb = math.floor(newAttenSteps + 0.5) * 3  -- Round to nearest 3 dB
        if newAttenDb ~= rfAttenDb then
            rfAttenDb = newAttenDb
            twin.setAttenuation(rfAttenDb)
        end
        local attenText = string.format("%d dB", rfAttenDb)
        drawText(lx + w - 60, ly - 2, attenText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

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
        -- Logarithmic volume: slider position 0-1 maps to -60dB to 0dB
        -- volume = 10^(dB/20), so position p: dB = -60 + 60*p, gain = 10^((-60+60*p)/20)
        local function linearToLog(pos)
            if pos <= 0 then return 0 end
            local db = -60 + 60 * pos  -- -60dB to 0dB
            return math.pow(10, db / 20)
        end
        local function logToLinear(gain)
            if gain <= 0.001 then return 0 end  -- Below -60dB, show as 0
            local db = 20 * math.log(gain, 10)
            return clamp((db + 60) / 60, 0, 1)
        end
        -- Use config.volume instead of local variable (demonstrates config API)
        local sliderPos = logToLinear(config.volume)
        local newSliderPos = ui.slider("volume", lx + 40, ly, w - 70, 0, 1, sliderPos)
        if newSliderPos ~= sliderPos then
            config.volume = linearToLog(newSliderPos)  -- Sets config, triggers C++ callback
        end
        local volDb = config.volume > 0.001 and string.format("%.0fdB", 20 * math.log(config.volume, 10)) or "-inf"
        drawText(lx + w - 60, ly - 2, volDb, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Sql:")
        squelch = ui.slider("squelch", lx + 40, ly, w - 70, 0, 1, squelch)
        layout.newLine(24)

        -- Mute checkbox (using config API)
        cx, cy = layout.getCursor()
        local newMuted = ui.checkbox("mute", "Mute", cx, cy, config.muted)
        if newMuted ~= config.muted then
            config.muted = newMuted  -- Triggers C++ callback
        end
        layout.newLine(24)

        -- Test tone button (using config API)
        local toneTags = config.testToneEnabled and {"Primary"} or {"Secondary"}
        local toneLabel = config.testToneEnabled and "Tone ON" or "Test Tone"
        local tx, ty = layout.getCursor()
        if ui.button("test_tone", toneLabel, tx, ty, 90, 26, toneTags) then
            config.testToneEnabled = not config.testToneEnabled  -- Triggers C++ callback
        end
        layout.newLine(28)

        -- WAV Recording button
        local isRecording = audio.isRecording()
        local recTags = isRecording and {"Danger"} or {"Secondary"}
        local recLabel = isRecording and "REC" or "Record"
        local rx, ry = layout.getCursor()
        if ui.button("wav_rec", recLabel, rx, ry, 70, 26, recTags) then
            if isRecording then
                audio.stopRecording()
            else
                audio.startRecording("/tmp/nexrx_audio.wav")
            end
        end
        -- Show recording duration
        if isRecording then
            local duration = audio.getRecordingDuration()
            local durText = string.format("%.1fs", duration)
            drawText(rx + 78, ry + 6, durText, 1.0, 0.3, 0.3, 1.0)
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
        local toggleX, toggleY = layout.getCursor()
        local rxLabel = rxActive and "RX ON" or "RX OFF"
        local rxTags = rxActive and {"Primary"} or {"Danger"}
        rxActive = ui.toggle("rx_toggle", rxLabel, toggleX, toggleY, w - 24, 40, rxActive, rxTags)
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

        -- Get real signal level from rx
        local rms, dBm, sUnits, dBoverS9 = rx.getSignalLevel()

        -- Map S-units to bar display (0-9 = bars 0-8, 9+ = bars 9+)
        -- Bar 0-8 = S1-S9, bar 9 = S9+10, etc
        local barCount = 12  -- S1-S9 + S9+10, +20, +30
        local barW = (mw - 8) / barCount - 2
        for i = 0, barCount - 1 do
            local barX = mx + 4 + i * (barW + 2)
            local barThreshold
            if i < 9 then
                barThreshold = i + 1  -- S1 through S9
            else
                barThreshold = 9 + (i - 8) * (10/6)  -- S9+10, +20, +30
            end

            if sUnits >= barThreshold then
                local r, g, b = 0.2, 0.8, 0.3  -- Green for S1-S5
                if i >= 9 then r, g, b = 0.9, 0.3, 0.2 end  -- Red for S9+
                if i >= 6 and i < 9 then r, g, b = 0.9, 0.7, 0.2 end  -- Yellow for S6-S9
                drawRect(barX, my + 4, barW, 20, r, g, b, 1.0)
            else
                drawRect(barX, my + 4, barW, 20, 0.2, 0.2, 0.25, 1.0)
            end
        end
        layout.newLine(40)

        -- Display S-meter reading
        local sText
        if sUnits < 9 then
            sText = string.format("S%.0f", math.max(1, math.floor(sUnits + 0.5)))
        else
            sText = string.format("S9+%.0f", math.max(0, dBoverS9))
        end
        local dBmText = string.format("%.0f dBm", dBm)
        local sx, sy = layout.center(measureText(sText), 16)
        drawText(sx - 20, layout.getCursor() - 8, sText, 0.9, 0.9, 0.95, 1.0)
        drawText(sx + 30, layout.getCursor() - 8, dBmText, 0.5, 0.5, 0.55, 1.0)
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

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator(sepX, sepY, w - 24)
        layout.newLine(12)

        -- Waterfall Range
        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Display", {"Title"})
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Floor:")
        local newMinDb = ui.slider("wf_min", lx + 45, ly, w - 75, -140, -60, wfMinDb)
        if newMinDb ~= wfMinDb then
            wfMinDb = newMinDb
            waterfall.setRange(wfMinDb, wfMaxDb)
        end
        local minText = string.format("%.0f dB", wfMinDb)
        drawText(lx + w - 60, ly - 2, minText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Peak:")
        local newMaxDb = ui.slider("wf_max", lx + 45, ly, w - 75, -100, -20, wfMaxDb)
        if newMaxDb ~= wfMaxDb then
            wfMaxDb = newMaxDb
            waterfall.setRange(wfMinDb, wfMaxDb)
        end
        local maxText = string.format("%.0f dB", wfMaxDb)
        drawText(lx + w - 60, ly - 2, maxText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)
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

        -- Title bar with frequency info (or entry field)
        local freqStr
        local freqColor = {0.2, 0.9, 0.4}
        if freqEntryMode then
            -- Show entry text with blinking cursor
            local cursor = freqEntryBlink < 0.5 and "_" or ""
            freqStr = freqEntryText .. cursor .. " MHz [Enter=OK, Esc=Cancel]"
            freqColor = {1.0, 0.9, 0.2}  -- Yellow when editing
        else
            freqStr = string.format("%.6f MHz", frequency)
        end
        drawText(px, py, freqStr, freqColor[1], freqColor[2], freqColor[3], 1.0)

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
