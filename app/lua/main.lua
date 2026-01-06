--[[
  NexRx Application - Main Lua Entry Point

  This file is loaded by the C++ host and provides:
  - init()   : Called once at startup
  - update() : Called each frame with delta time
  - draw()   : Called each frame for rendering
]]

-- Global state
local frameCount = 0
local mouseX, mouseY = 0, 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0

-- Demo state
local buttonHovered = false
local buttonPressed = false
local sliderValue = 0.5
local frequency = 14.200  -- MHz

-- Helper: Check if point is inside rectangle
local function pointInRect(px, py, x, y, w, h)
    return px >= x and px < x + w and py >= y and py < y + h
end

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

-- Called once at startup
function init()
    print("[Lua] init() called")

    -- Set background color from SetBox
    local bg = setbox.getString("background", "#1a1a2e")
    local r, g, b = hexToRgb(bg)
    setClearColor(r, g, b)

    print("[Lua] NexRx UI initialized with text rendering")
end

-- Called each frame
function update(dt)
    frameCount = frameCount + 1
    mouseX, mouseY = getMousePos()

    -- FPS calculation
    fpsAccum = fpsAccum + dt
    fpsFrames = fpsFrames + 1
    if fpsAccum >= 1.0 then
        fps = fpsFrames / fpsAccum
        fpsAccum = 0
        fpsFrames = 0
    end

    -- Update button state
    local btnX, btnY, btnW, btnH = 50, 60, 150, 36
    buttonHovered = pointInRect(mouseX, mouseY, btnX, btnY, btnW, btnH)

    if buttonHovered and isMouseClicked(0) then
        buttonPressed = not buttonPressed
        print("[Lua] Button toggled: " .. tostring(buttonPressed))
    end

    -- Update slider (frequency control)
    local sliderX, sliderY, sliderW, sliderH = 50, 140, 200, 8
    if isMouseDown(0) and pointInRect(mouseX, mouseY, sliderX - 10, sliderY - 10, sliderW + 20, sliderH + 20) then
        sliderValue = clamp((mouseX - sliderX) / sliderW, 0, 1)
        frequency = 1.0 + sliderValue * 29.0  -- 1-30 MHz
    end

    -- Mouse wheel for fine tuning
    local wheel = getMouseWheel()
    if wheel ~= 0 then
        frequency = clamp(frequency + wheel * 0.01, 1.0, 30.0)
        sliderValue = (frequency - 1.0) / 29.0
    end
end

-- Called each frame for rendering
function draw()
    local winW, winH = getWindowSize()
    local lineH = getLineHeight()

    -- Main panel
    drawRoundedRect(30, 30, 280, 260, 8, 0.12, 0.12, 0.18, 1.0)
    drawRectOutline(30, 30, 280, 260, 0.3, 0.3, 0.4, 1.0, 1.0)

    -- Panel title
    drawText(50, 40, "NexRx Control Panel", 0.9, 0.9, 0.95, 1.0)

    -- Button with rounded corners
    local btnX, btnY, btnW, btnH = 50, 70, 150, 36
    local btnR, btnG, btnB = 0.23, 0.51, 0.96

    if buttonPressed then
        btnR, btnG, btnB = 0.11, 0.31, 0.85
    elseif buttonHovered then
        btnR, btnG, btnB = 0.38, 0.65, 0.98
    end

    drawRoundedRect(btnX, btnY, btnW, btnH, 6, btnR, btnG, btnB, 1.0)

    -- Button label (centered)
    local label = buttonPressed and "RX Active" or "RX Off"
    local labelW = measureText(label)
    drawText(btnX + (btnW - labelW) / 2, btnY + (btnH - lineH) / 2, label, 1.0, 1.0, 1.0, 1.0)

    -- Frequency display
    drawText(50, 120, "Frequency:", 0.7, 0.7, 0.75, 1.0)
    local freqStr = string.format("%.3f MHz", frequency)
    drawText(140, 120, freqStr, 0.2, 0.9, 0.4, 1.0)

    -- Slider
    local sliderX, sliderY, sliderW, sliderH = 50, 150, 200, 8
    drawRoundedRect(sliderX, sliderY, sliderW, sliderH, 4, 0.25, 0.25, 0.3, 1.0)

    -- Slider fill
    local fillW = sliderW * sliderValue
    if fillW > 0 then
        drawRoundedRect(sliderX, sliderY, fillW, sliderH, 4, 0.23, 0.51, 0.96, 1.0)
    end

    -- Slider handle
    local handleX = sliderX + fillW - 8
    local handleY = sliderY - 6
    drawCircle(sliderX + fillW, sliderY + sliderH/2, 10, 1.0, 1.0, 1.0, 1.0)
    drawCircleOutline(sliderX + fillW, sliderY + sliderH/2, 10, 0.3, 0.3, 0.4, 1.0, 2.0)

    -- Band indicator
    drawText(50, 175, "Band:", 0.7, 0.7, 0.75, 1.0)
    local band = "?"
    if frequency >= 1.8 and frequency <= 2.0 then band = "160m"
    elseif frequency >= 3.5 and frequency <= 4.0 then band = "80m"
    elseif frequency >= 7.0 and frequency <= 7.3 then band = "40m"
    elseif frequency >= 14.0 and frequency <= 14.35 then band = "20m"
    elseif frequency >= 21.0 and frequency <= 21.45 then band = "15m"
    elseif frequency >= 28.0 and frequency <= 29.7 then band = "10m"
    else band = "Out of band"
    end
    drawText(100, 175, band, 0.9, 0.7, 0.2, 1.0)

    -- S-Meter placeholder
    drawText(50, 210, "S-Meter:", 0.7, 0.7, 0.75, 1.0)
    drawRoundedRect(50, 230, 200, 20, 4, 0.15, 0.15, 0.2, 1.0)

    -- Fake S-meter bars
    local sLevel = math.abs(math.sin(frameCount * 0.02)) * 0.7 + 0.2
    for i = 0, 9 do
        local barX = 54 + i * 19
        local barH = 12
        local intensity = (i + 1) / 10
        if intensity <= sLevel then
            local r, g, b = 0.2, 0.8, 0.3
            if i >= 7 then r, g, b = 0.9, 0.3, 0.2 end
            if i >= 5 and i < 7 then r, g, b = 0.9, 0.7, 0.2 end
            drawRect(barX, 234, 15, barH, r, g, b, 1.0)
        else
            drawRect(barX, 234, 15, barH, 0.25, 0.25, 0.3, 1.0)
        end
    end

    -- Instructions panel
    drawRoundedRect(30, 310, 280, 80, 8, 0.12, 0.12, 0.18, 1.0)
    drawText(50, 320, "Controls:", 0.8, 0.8, 0.85, 1.0)
    drawText(50, 340, "- Click button to toggle RX", 0.6, 0.6, 0.65, 1.0)
    drawText(50, 358, "- Drag slider or scroll wheel", 0.6, 0.6, 0.65, 1.0)
    drawText(50, 376, "- ESC to quit", 0.6, 0.6, 0.65, 1.0)

    -- Status bar at bottom
    drawRect(0, winH - 28, winW, 28, 0.08, 0.08, 0.1, 1.0)
    drawLine(0, winH - 28, winW, winH - 28, 0.3, 0.3, 0.35, 1.0, 1.0)

    -- Status text
    local statusText = string.format("FPS: %.0f | Mouse: %d, %d | Frame: %d",
                                      fps, mouseX, mouseY, frameCount)
    drawText(10, winH - 22, statusText, 0.6, 0.6, 0.65, 1.0)

    -- NexRx branding
    local brand = "NexRx v0.1"
    local brandW = measureText(brand)
    drawText(winW - brandW - 10, winH - 22, brand, 0.4, 0.5, 0.7, 1.0)
end

print("[Lua] main.lua loaded")
