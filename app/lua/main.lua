--[[
  NexRx Application - Main Lua Entry Point

  Demonstrates the layout system with a realistic SDR receiver UI.

  Architecture: Lua is the control plane, C++ is the data plane.
  - Configuration loaded from SetBox (config/default.lua, config/settings.lua)
  - Lua owns all state and UI logic
  - Lua calls C++ primitives for DSP, audio, rendering
]]

-- Add lua directory to package path for requires
local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path

-- Load UI modules
local ui = require("ui.widgets")
local theme = require("ui.theme")
local layout = require("ui.layout")

-- Load mode definitions (Lua owns mode enum)
local modeHelper = require("modes")

-- Load S-meter calculations (Lua owns signal calculations)
local smeter = require("smeter")

-- Global state
local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0

-- Application state (all loaded from SetBox in init())
local rxActive = false

-- Audio state (loaded from SetBox, controls C++ audio engine)
local volumeDb          -- dB, range -60 to 0 (loaded from SetBox)
local muted             -- bool (loaded from SetBox)
local testToneEnabled   -- bool (test tone generation)

-- Baseband filter state (loaded from SetBox, controls C++ BasebandFilter)
local bandpassEnabled   -- bool
local bandpassCenter    -- Hz offset from DC
local bandpassWidth     -- Hz bandwidth
local notchEnabled      -- bool
local notchCenter       -- Hz offset from DC
local notchWidth        -- Hz bandwidth
local frequency        -- MHz (loaded from SetBox)
local squelch          -- 0-1 (loaded from SetBox)
local agcEnabled       -- bool (loaded from SetBox)
local nrEnabled        -- bool (loaded from SetBox)
local nbEnabled        -- bool (loaded from SetBox)
local selectedMode     -- "USB", "LSB", "CW", "AM" (loaded from SetBox)
local selectedBand     -- "20m", etc. (loaded from SetBox)

-- VFO state (loaded from SetBox)
local vfoA             -- MHz
local vfoB             -- MHz
local activeVFO        -- "A" or "B"

-- LMS adaptive filter (loaded from SetBox)
local lmsMu            -- Learning rate for image rejection

-- QSD offset k (kHz) for image rejection (loaded from SetBox)
local qsdOffsetK       -- kHz

-- RF Attenuator (0-45 dB in 3 dB steps) (loaded from SetBox)
local rfAttenDb        -- dB

-- Waterfall state (loaded from SetBox)
local wfBins           -- FFT bins
local wfRows           -- history rows
local spectrumData = {}  -- runtime buffer
local wfColormap       -- colormap name
local wfMinDb          -- Waterfall min dB (noise floor)
local wfMaxDb          -- Waterfall max dB (strongest signals)

-- Hardware connection state (loaded from SetBox)
local hwSettings = {
    host = nil,
    controlPort = nil,
    streamPort = nil,
}

-- Connect to hardware with current settings
local function connectHardware()
    if hw.isConnected() then
        hw.disconnect()
    end

    print("[Lua] Connecting to hardware at " .. hwSettings.host ..
          " (TCP:" .. hwSettings.controlPort .. ", UDP:" .. hwSettings.streamPort .. ")...")

    if hw.connect(hwSettings.host, hwSettings.controlPort, hwSettings.streamPort) then
        hwConnected = true
        print("[Lua] Connected to hardware!")
        -- Send initial QSD offset k
        hw.setQsdOffset(qsdOffsetK)
        print("[Lua] Set QSD offset k = " .. qsdOffsetK .. " kHz")
        return true
    else
        hwConnected = false
        print("[Lua] Failed to connect to hardware")
        return false
    end
end

-- Disconnect from hardware
local function disconnectHardware()
    if hw.isConnected() then
        hw.disconnect()
        hwConnected = false
        print("[Lua] Disconnected from hardware")
    end
end

-- Hardware connection state
local hwConnected = false
local hwFramesReceived = 0
local useHwSpectrum = true  -- Try to use hardware data when available
local lastVfoFreq = 0  -- Track VFO changes for hardware control

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

-- Apply colormap by name from Lua definitions (config/colormaps.lua)
-- Colormaps are defined in Lua only - no C++ fallback
local function applyColormap(name)
    -- Check if colormaps table exists (loaded from config/colormaps.lua)
    if colormaps and colormaps[name] then
        waterfall.setColormapData(colormaps[name])
        return true
    else
        print("[Lua] Warning: Colormap '" .. name .. "' not found in config/colormaps.lua")
        return false
    end
end

-- Initialize
function init()
    print("[Lua] init() called")

    -- ==========================================================================
    -- Load all configuration from SetBox
    -- ==========================================================================

    -- Radio settings
    frequency = setbox.getNumber("defaultFrequency", 14.200e6) / 1e6  -- Convert Hz to MHz
    selectedMode = setbox.getString("defaultMode", "USB")
    selectedBand = setbox.getString("defaultBand", "20m")

    -- VFO state
    vfoA = setbox.getNumber("vfoA", 14.200e6) / 1e6  -- Convert Hz to MHz
    vfoB = setbox.getNumber("vfoB", 7.050e6) / 1e6   -- Convert Hz to MHz
    activeVFO = setbox.getString("activeVFO", "A")

    -- DSP settings
    squelch = setbox.getNumber("squelch", 0.3)
    agcEnabled = setbox.getBool("agcEnabled", true)
    nrEnabled = setbox.getBool("nrEnabled", false)
    nbEnabled = setbox.getBool("nbEnabled", false)
    lmsMu = setbox.getNumber("lmsMu", 0.001)

    -- Audio settings (volume in dB for intuitive config)
    volumeDb = setbox.getNumber("volumeDb", -20)
    muted = setbox.getBool("muted", false)
    testToneEnabled = false  -- Always start with test tone off

    -- Baseband filter settings (offset from SetBox, or defaults)
    bandpassEnabled = setbox.getBool("bandpassEnabled", false)
    bandpassCenter = setbox.getNumber("bandpassCenter", 700)
    bandpassWidth = setbox.getNumber("bandpassWidth", 500)
    notchEnabled = setbox.getBool("notchEnabled", false)
    notchCenter = setbox.getNumber("notchCenter", 0)
    notchWidth = setbox.getNumber("notchWidth", 100)

    -- Hardware settings
    qsdOffsetK = setbox.getNumber("qsdOffsetK", 12.0)
    rfAttenDb = math.floor(setbox.getNumber("rfAttenDb", 0))

    -- Waterfall/display settings
    wfBins = math.floor(setbox.getNumber("wfBins", 512))
    wfRows = math.floor(setbox.getNumber("wfRows", 256))
    wfColormap = setbox.getString("wfColormap", "viridis")
    wfMinDb = setbox.getNumber("wfMinDb", -120)
    wfMaxDb = setbox.getNumber("wfMaxDb", -40)

    -- Hardware connection settings
    hwSettings.host = setbox.getString("hwHost", "127.0.0.1")
    hwSettings.controlPort = math.floor(setbox.getNumber("hwControlPort", 5000))
    hwSettings.streamPort = math.floor(setbox.getNumber("hwStreamPort", 5001))

    print(string.format("[Lua] Config loaded: freq=%.3f MHz, mode=%s, band=%s",
        frequency, selectedMode, selectedBand))
    print(string.format("[Lua] VFO A=%.3f MHz, B=%.3f MHz, active=%s",
        vfoA, vfoB, activeVFO))
    print(string.format("[Lua] DSP: squelch=%.2f, AGC=%s, NR=%s, NB=%s",
        squelch, tostring(agcEnabled), tostring(nrEnabled), tostring(nbEnabled)))
    print(string.format("[Lua] Hardware: QSD k=%.1f kHz, atten=%d dB",
        qsdOffsetK, rfAttenDb))
    print(string.format("[Lua] Display: waterfall %dx%d, colormap=%s, range=[%d,%d] dB",
        wfBins, wfRows, wfColormap, wfMinDb, wfMaxDb))

    -- ==========================================================================
    -- Initialize subsystems
    -- ==========================================================================

    -- Background color
    local bg = setbox.getString("background", "#1a1a2e")
    local r, g, b = hexToRgb(bg)
    setClearColor(r, g, b)

    -- Initialize audio (convert dB to linear gain for C++ audio engine)
    if audio.isInitialized() then
        local linearGain = math.pow(10, volumeDb / 20)
        audio.setVolume(linearGain)
        audio.setMuted(muted)
        audio.start()
        print("[Lua] Audio started at " .. audio.getSampleRate() .. " Hz")
    else
        print("[Lua] Warning: Audio not initialized")
    end

    -- Initialize baseband filter with current settings
    rx.setBandpassEnabled(bandpassEnabled)
    rx.setBandpassCenter(bandpassCenter)
    rx.setBandpassWidth(bandpassWidth)
    rx.setNotchEnabled(notchEnabled)
    rx.setNotchCenter(notchCenter)
    rx.setNotchWidth(notchWidth)

    -- Initialize waterfall
    if waterfall.init(wfBins, wfRows) then
        waterfall.setRange(wfMinDb, wfMaxDb)
        applyColormap(wfColormap)
        print("[Lua] Waterfall initialized: " .. wfBins .. "x" .. wfRows)
    else
        print("[Lua] Warning: Waterfall init failed")
    end

    -- Initialize spectrum data buffer
    for i = 1, wfBins do
        spectrumData[i] = -100
    end

    -- Try to connect to hardware (digital twin simulation or real hardware)
    local autoConnect = setbox.getBool("hwAutoConnect", true)
    if autoConnect then
        if not connectHardware() then
            print("[Lua] Hardware not available - using simulated spectrum")
        end
    else
        hwConnected = false
        print("[Lua] Hardware auto-connect disabled")
    end

    print("[Lua] NexRx UI initialized with layout system")
end

-- Generate simulated spectrum data
local function generateSpectrum()
    local centerBin = wfBins / 2
    for i = 1, wfBins do
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

    -- Get spectrum data from hardware or generate simulated
    local gotHwData = false
    if useHwSpectrum and hw.isConnected() then
        local hwSpectrum = hw.getSpectrum()
        if #hwSpectrum > 0 then
            -- Resample hardware spectrum to our bin count if needed
            if #hwSpectrum == wfBins then
                spectrumData = hwSpectrum
            else
                -- Simple resampling
                local ratio = #hwSpectrum / wfBins
                for i = 1, wfBins do
                    local srcIdx = math.floor((i - 1) * ratio) + 1
                    spectrumData[i] = hwSpectrum[srcIdx] or -100
                end
            end
            gotHwData = true
            hwFramesReceived = hw.getFramesReceived()
        end
        hwConnected = true
    else
        hwConnected = hw.isConnected()
    end

    -- Fall back to simulated spectrum if no hardware data
    if not gotHwData then
        generateSpectrum()
    end

    -- Update waterfall
    if waterfall.isInitialized() then
        waterfall.addRow(spectrumData)
    end

    -- Send VFO changes to hardware (frequency is in MHz, convert to Hz)
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

        -- Hardware connection status
        local hwStatus = hwConnected and "HW" or "SIM"
        local hwStatusColor = hwConnected and {0.2, 0.9, 0.4} or {0.6, 0.6, 0.6}
        drawText(x + 200, y + 8, hwStatus, hwStatusColor[1], hwStatusColor[2], hwStatusColor[3], 1.0)
        if hwConnected then
            local framesText = string.format(" (%d frames)", hwFramesReceived)
            drawText(x + 200 + measureText(hwStatus) + 4, y + 8, framesText, 0.5, 0.5, 0.55, 1.0)

            -- Audio stats for debugging (now includes drops and fill ratio)
            local audioWritten, audioRead, underruns, drops, fillRatio = rx.getAudioStats()
            local audioText = string.format("Buf: %.0f%%", fillRatio * 100)
            drawText(x + 400, y + 8, audioText, 0.5, 0.7, 0.5, 1.0)

            -- Drop rates (IQ and Audio)
            local iqDropRate = hw.getIqDropRate()
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

        local bpInfo = bandpassEnabled and string.format("BP: %.0f Hz", bandpassWidth) or "BP: Off"
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
                modeHelper.setMode(mode)  -- Update demodulator mode
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
        local newBpEn = ui.checkbox("bp_en", "Bandpass", cx, cy, bandpassEnabled)
        if newBpEn ~= bandpassEnabled then
            bandpassEnabled = newBpEn
            rx.setBandpassEnabled(bandpassEnabled)
        end
        layout.newLine(24)

        -- Bandpass center (only show if enabled)
        if bandpassEnabled then
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Center:")
            local newBpCenter = ui.slider("bp_center", lx + 55, ly, w - 85, -5000, 5000, bandpassCenter)
            if newBpCenter ~= bandpassCenter then
                bandpassCenter = newBpCenter
                rx.setBandpassCenter(bandpassCenter)
            end
            local bpCText = string.format("%.0f Hz", bandpassCenter)
            drawText(lx + w - 70, ly - 2, bpCText, 0.5, 0.5, 0.55, 1.0)
            layout.newLine(24)

            -- Bandpass width
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Width:")
            local newBpWidth = ui.slider("bp_width", lx + 55, ly, w - 85, 50, 4000, bandpassWidth)
            if newBpWidth ~= bandpassWidth then
                bandpassWidth = newBpWidth
                rx.setBandpassWidth(bandpassWidth)
            end
            local bpWText = string.format("%.0f Hz", bandpassWidth)
            drawText(lx + w - 70, ly - 2, bpWText, 0.5, 0.5, 0.55, 1.0)
            layout.newLine(24)
        end

        -- Notch enable
        cx, cy = layout.getCursor()
        local newNotchEn = ui.checkbox("notch_en", "Notch", cx, cy, notchEnabled)
        if newNotchEn ~= notchEnabled then
            notchEnabled = newNotchEn
            rx.setNotchEnabled(notchEnabled)
        end
        layout.newLine(24)

        -- Notch center (only show if enabled)
        if notchEnabled then
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Center:")
            local newNotchCenter = ui.slider("notch_center", lx + 55, ly, w - 85, -10000, 10000, notchCenter)
            if newNotchCenter ~= notchCenter then
                notchCenter = newNotchCenter
                rx.setNotchCenter(notchCenter)
            end
            local notchCText = string.format("%.0f Hz", notchCenter)
            drawText(lx + w - 70, ly - 2, notchCText, 0.5, 0.5, 0.55, 1.0)
            layout.newLine(24)

            -- Notch width
            lx, ly = layout.getCursor()
            ui.label(lx, ly, "Width:")
            local newNotchWidth = ui.slider("notch_width", lx + 55, ly, w - 85, 10, 500, notchWidth)
            if newNotchWidth ~= notchWidth then
                notchWidth = newNotchWidth
                rx.setNotchWidth(notchWidth)
            end
            local notchWText = string.format("%.0f Hz", notchWidth)
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
            hw.setQsdOffset(qsdOffsetK)
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
            hw.setAttenuation(rfAttenDb)
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
        -- Volume slider works directly in dB (-60 to 0)
        local newVolumeDb = ui.slider("volume", lx + 40, ly, w - 70, -60, 0, volumeDb)
        if newVolumeDb ~= volumeDb then
            volumeDb = newVolumeDb
            local linearGain = math.pow(10, volumeDb / 20)
            audio.setVolume(linearGain)
        end
        local volText = string.format("%.0fdB", volumeDb)
        drawText(lx + w - 60, ly - 2, volText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label(lx, ly, "Sql:")
        squelch = ui.slider("squelch", lx + 40, ly, w - 70, 0, 1, squelch)
        layout.newLine(24)

        -- Mute checkbox
        cx, cy = layout.getCursor()
        local newMuted = ui.checkbox("mute", "Mute", cx, cy, muted)
        if newMuted ~= muted then
            muted = newMuted
            audio.setMuted(muted)
        end
        layout.newLine(24)

        -- Test tone button
        local toneTags = testToneEnabled and {"Primary"} or {"Secondary"}
        local toneLabel = testToneEnabled and "Tone ON" or "Test Tone"
        local tx, ty = layout.getCursor()
        if ui.button("test_tone", toneLabel, tx, ty, 90, 26, toneTags) then
            testToneEnabled = not testToneEnabled
            audio.setTestTone(testToneEnabled)
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

        -- Get signal level from smeter module (Lua calculates from RMS)
        local sig = smeter.getReading()

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

            if sig.sUnits >= barThreshold then
                local r, g, b = 0.2, 0.8, 0.3  -- Green for S1-S5
                if i >= 9 then r, g, b = 0.9, 0.3, 0.2 end  -- Red for S9+
                if i >= 6 and i < 9 then r, g, b = 0.9, 0.7, 0.2 end  -- Yellow for S6-S9
                drawRect(barX, my + 4, barW, 20, r, g, b, 1.0)
            else
                drawRect(barX, my + 4, barW, 20, 0.2, 0.2, 0.25, 1.0)
            end
        end
        layout.newLine(40)

        -- Display S-meter reading (text comes from smeter module)
        local sx, sy = layout.center(measureText(sig.sText), 16)
        drawText(sx - 20, layout.getCursor() - 8, sig.sText, 0.9, 0.9, 0.95, 1.0)
        drawText(sx + 30, layout.getCursor() - 8, sig.dBmText, 0.5, 0.5, 0.55, 1.0)
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
        -- Uses colormaps from config/colormaps.lua when available
        local cmapNames = {"viridis", "plasma", "inferno", "green", "blue"}
        local cmapX = px + pw - 280
        for i, cmap in ipairs(cmapNames) do
            local btnX = cmapX + (i - 1) * 55
            local tags = wfColormap == cmap and {"Active"} or {}
            if ui.button("cmap_" .. cmap, cmap, btnX, py - 2, 52, 20, tags) then
                wfColormap = cmap
                applyColormap(cmap)
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
