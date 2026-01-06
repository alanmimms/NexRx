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

-- Demo state
local buttonHovered = false
local buttonPressed = false
local sliderValue = 0.5

-- Colors (will come from SetBox eventually)
local colors = {
    background = {0.1, 0.1, 0.15},
    panel = {0.15, 0.15, 0.2},
    button = {0.23, 0.51, 0.96},
    buttonHover = {0.38, 0.65, 0.98},
    buttonPress = {0.11, 0.31, 0.85},
    text = {1.0, 1.0, 1.0},
    slider = {0.3, 0.3, 0.35},
    sliderFill = {0.23, 0.51, 0.96},
}

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

-- Called once at startup
function init()
    print("[Lua] init() called")

    -- Set background color from SetBox
    local bg = setbox.getString("background", "#1a1a2e")
    local r, g, b = hexToRgb(bg)
    setClearColor(r, g, b)

    print("[Lua] NexRx UI initialized")
end

-- Called each frame
function update(dt)
    frameCount = frameCount + 1
    mouseX, mouseY = getMousePos()

    -- Update button state
    local btnX, btnY, btnW, btnH = 50, 50, 150, 40
    buttonHovered = pointInRect(mouseX, mouseY, btnX, btnY, btnW, btnH)

    if buttonHovered and isMouseClicked(0) then
        buttonPressed = true
        print("[Lua] Button clicked!")
    end

    if isMouseClicked(0) and not buttonHovered then
        buttonPressed = false
    end

    -- Update slider
    local sliderX, sliderY, sliderW, sliderH = 50, 120, 200, 20
    if isMouseDown(0) and pointInRect(mouseX, mouseY, sliderX, sliderY - 5, sliderW, sliderH + 10) then
        sliderValue = math.max(0, math.min(1, (mouseX - sliderX) / sliderW))
    end
end

-- Called each frame for rendering
function draw()
    local winW, winH = getWindowSize()

    -- Draw a panel
    drawRect(30, 30, 250, 200, 0.15, 0.15, 0.2, 1.0)
    drawRectOutline(30, 30, 250, 200, 0.3, 0.3, 0.35, 1.0, 1.0)

    -- Draw button
    local btnX, btnY, btnW, btnH = 50, 50, 150, 40
    local btnColor = colors.button
    if buttonPressed then
        btnColor = colors.buttonPress
    elseif buttonHovered then
        btnColor = colors.buttonHover
    end
    drawRect(btnX, btnY, btnW, btnH, btnColor[1], btnColor[2], btnColor[3], 1.0)

    -- Draw slider track
    local sliderX, sliderY, sliderW, sliderH = 50, 120, 200, 8
    drawRect(sliderX, sliderY, sliderW, sliderH, colors.slider[1], colors.slider[2], colors.slider[3], 1.0)

    -- Draw slider fill
    local fillW = sliderW * sliderValue
    drawRect(sliderX, sliderY, fillW, sliderH, colors.sliderFill[1], colors.sliderFill[2], colors.sliderFill[3], 1.0)

    -- Draw slider handle
    local handleX = sliderX + fillW - 6
    local handleY = sliderY - 4
    drawRect(handleX, handleY, 12, 16, 1.0, 1.0, 1.0, 1.0)

    -- Draw info panel
    drawRect(30, 250, 250, 100, 0.15, 0.15, 0.2, 1.0)

    -- Draw mouse position indicator
    drawRect(mouseX - 5, mouseY - 5, 10, 10, 1.0, 0.5, 0.0, 0.8)

    -- Status area at bottom
    drawRect(0, winH - 30, winW, 30, 0.1, 0.1, 0.12, 1.0)
end

print("[Lua] main.lua loaded")
