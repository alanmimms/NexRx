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
local uiState = require("ui.state")
local constraints = require("ui.constraints")
local container = require("ui.container")

-- Load new architecture modules
_G.bands = require("bands")
local bands = _G.bands
local dispatch = require("dispatch")
local events = require("events")
local animate = require("animate")
local keys = require("keycodes")

-- Load mode definitions (Lua owns mode enum)
local modeHelper = require("modes")

-- Load S-meter calculations (Lua owns signal calculations)
local smeter = require("smeter")

-- Load reactive property system
local R = require("reactive")
local AppState = require("app_state")
local Edit = require("edit")
local layoutOverrides = require("layout_overrides")

-- Global state
local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0

-- Mouse tracking for motion events
local lastMouseX, lastMouseY = 0, 0

-- Button names for tags (SDL: 0=left, 1=middle, 2=right)
-- Use UPPERCASE as per unified tag architecture
local BUTTON_NAMES = {"LEFT", "MIDDLE", "RIGHT"}

-- Application state (bridged to AppState)
-- Most state is in AppState, but we keep a local proxy for compatibility
local state = setmetatable({}, {
    __index = function(_, k)
        return AppState.get(k)
    end,
    __newindex = function(_, k, v)
        AppState.set(k, v)
    end
})

-- Get/set property values (bridged to AppState)
local function getProperty(name)
    return AppState.get(name)
end

local function setProperty(name, value)
    -- Check SetBox if this property should be animated
    local prevTags = setbox.getActiveTags()
    setbox.addTag("prop." .. name)
    local isAnimated = setbox.getBool("animated", false)
    setbox.setActiveTags(prevTags)

    if isAnimated then
        AppState.animateTo(name, value)
    else
        AppState.set(name, value)
    end
end

-- Non-property state (not exposed via AppState)
local wfBins           -- FFT bins (read-only after init)
local wfRows           -- history rows (read-only after init)
local spectrumData = {}  -- runtime buffer

-- Key state tracking for KeyUp detection
local keyStates = {}     -- scancode -> true if key was down last frame
local activeTags = {}    -- tag name -> true for all currently active tags (held keys, held buttons, etc.)

-- Property accessor registry (not used anymore, kept as empty for compatibility)
local propertyAccessors = {}

-- Get ALL active tags for debugging - EVERYTHING in the system
-- Shows complete tag state: all widgets, all inputs, all modes
-- All tags use unified namespaces: input.*, state.*, widget.*, event.*
local function getAllActiveTags()
    local allTags = {}

    -- 1. Raw held keys and mouse buttons (namespaced as input.*)
    for tagName, _ in pairs(activeTags) do
        allTags[tagName] = true
    end

    -- 2. Derived generic modifiers
    if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then allTags["input.SHIFT"] = true end
    if activeTags["input.LCTRL"] or activeTags["input.RCTRL"] then allTags["input.CTRL"] = true end
    if activeTags["input.LALT"] or activeTags["input.RALT"] then allTags["input.ALT"] = true end

    -- 3. Mode tags from events module (state.FreqEntryMode, etc.)
    if events and events.getModeTags then
        for _, modeTag in ipairs(events.getModeTags()) do
            allTags[modeTag] = true
        end
    end

    -- 4. Tags of the HOVERED widget only
    local mouseX, mouseY = getMousePos()
    local hoveredWidget = events.getWidgetAt(mouseX, mouseY)
    if hoveredWidget then
        allTags["state.Hovered"] = true
        if hoveredWidget.id then
            allTags["state.Hovered:" .. hoveredWidget.id] = true
        end
        if hoveredWidget.tags then
            for _, tag in ipairs(hoveredWidget.tags) do
                allTags[tag] = true
            end
        end
    end

    -- 5. Pressed state
    if activeTags["input.MouseLEFT"] and hoveredWidget then
        allTags["state.Pressed"] = true
        if hoveredWidget.id then
            allTags["state.Pressed:" .. hoveredWidget.id] = true
        end
    end

    return allTags
end

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

        -- Enable hardware dispatch functions first
        dispatch.enableHardware()

        -- Sync current state to hardware (direct calls to ensure it happens)
        dispatch.setVfo(state.frequency * 1e6)
        dispatch.setRxActive(state.rxActive)
        
        -- Send initial QSD offset k
        hw.setQsdOffset(state.qsdOffsetK)
        print("[Lua] Set QSD offset k = " .. state.qsdOffsetK .. " kHz")
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
        -- Disable hardware dispatch functions
        dispatch.disableHardware()
        print("[Lua] Disconnected from hardware")
    end
end

-- Hardware connection state
local hwConnected = false
local hwFramesReceived = 0
local lastVfoFreq = 0  -- Track VFO changes for hardware control
-- Note: useHwSpectrum replaced by dispatch module

-- Frequency entry state (mode is managed via events.hasModeTag("state.FreqEntryMode"))
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

-- getBand() replaced by bands module - see bands.getCurrent()
-- Band definitions now in config/bands.lua as SetBox rules

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

    -- Ensure all config files are loaded (safe to call multiple times as rules are additive)
    local configFiles = {
        "config/bands.lua",
        "config/colormaps.lua",
        "config/events.lua",
        "config/layout.lua",
        "config/modes.lua",
        "config/constraints.lua",
    }
    for _, file in ipairs(configFiles) do
        setbox.loadFile(file)
    end

    -- Initialize reactive state system
    AppState.init()

    -- Load non-property display settings
    wfBins = math.floor(state.wfBins or 512)
    wfRows = math.floor(state.wfRows or 256)

    -- Hardware connection settings
    hwSettings.host = setbox.getString("hwHost", "127.0.0.1")
    hwSettings.controlPort = math.floor(setbox.getNumber("hwControlPort", 5000))
    hwSettings.streamPort = math.floor(setbox.getNumber("hwStreamPort", 5001))

    print(string.format("[Lua] Config loaded: freq=%.3f MHz, mode=%s, band=%s",
        state.frequency, state.selectedMode, state.selectedBand))

    -- ==========================================================================
    -- Initialize subsystems
    -- ==========================================================================

    -- Background color
    local bg = setbox.getString("background", "#1a1a2e")
    local r, g, b = hexToRgb(bg)
    setClearColor(r, g, b)

    -- Initialize audio
    if audio.isInitialized() then
        audio.start()
        print("[Lua] Audio started at " .. audio.getSampleRate() .. " Hz")
    else
        print("[Lua] Warning: Audio not initialized")
    end

    -- Initialize waterfall
    if waterfall.init(wfBins, wfRows) then
        waterfall.setRange(state.wfMinDb, state.wfMaxDb)
        applyColormap(state.wfColormap)
        dispatch.enableWaterfall()  -- Enable waterfall functions
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
        print("[Lua] Hardware auto-connect disabled")
    end

    -- ==========================================================================
    -- Initialize new architecture modules
    -- ==========================================================================

    -- Initialize bands module
    bands.init()
    bands.setCurrent(state.frequency * 1e6)

    -- Initialize events module and wire to layout/widgets
    events.init()
    layout.setEventsModule(events)
    ui.setEventsModule(events)
    ui.setLayoutModule(layout)

    -- =======================================================================
    -- State Watchers
    -- =======================================================================

    -- Watch for waterfall range changes
    AppState.watch("wfMinDb", function(v) waterfall.setRange(v, state.wfMaxDb) end)
    AppState.watch("wfMaxDb", function(v) waterfall.setRange(state.wfMinDb, v) end)
    AppState.watch("wfColormap", function(v) applyColormap(v) end)

    -- =======================================================================
    -- Generic Event Handlers
    -- =======================================================================

    -- Helper to check modifiers in event
    local function hasModifier(event, mod)
        if not event.modifiers then return false end
        for _, m in ipairs(event.modifiers) do
            if m == mod then return true end
        end
        return false
    end

    -- =======================================================================
    -- Slider Handler Factory
    -- Extracts common logic: widget validation, delta extraction, property update
    -- =======================================================================

    -- Factory for slider adjustment handlers
    -- defaultStepFn: function(current, delta, min, max, ctrl, shift) -> newValue (fallback)
    -- If props.step is provided by the rule, uses that instead of defaultStepFn
    local function createSliderHandler(defaultStepFn, handlerName)
        return function(event, widget, props)
            -- Validate widget has required data
            if not widget or not widget.data then return false end
            local data = widget.data
            local propName, minVal, maxVal = data.property, data.min, data.max
            if not propName or not minVal or not maxVal then return false end

            -- Determine delta from wheel or arrow keys
            local delta = event.delta
            if not delta then
                if event.key == "Right" or event.key == "Up" then delta = 1
                elseif event.key == "Left" or event.key == "Down" then delta = -1
                else return false end
            end
            if delta == 0 then return false end

            -- Get current value
            local current = getProperty(propName)
            if current == nil then return false end

            -- Calculate new value
            local newVal
            if props and props.step then
                -- Rule provided explicit step value - use it directly
                newVal = current + delta * props.step
            else
                -- Fall back to default step calculation
                local ctrl = hasModifier(event, "input.CTRL")
                local shift = hasModifier(event, "input.SHIFT")
                newVal = defaultStepFn(current, delta, minVal, maxVal, ctrl, shift)
            end
            newVal = math.max(minVal, math.min(maxVal, newVal))

            -- Debug output
            if events.debugDispatch then
                local modsStr = event.modifiers and table.concat(event.modifiers, ",") or "none"
                local stepInfo = (props and props.step) and string.format("step=%s", props.step) or "default"
                print(string.format("[%s] prop=%s current=%s new=%s mods={%s} %s",
                    handlerName, propName, tostring(current), tostring(newVal), modsStr, stepInfo))
            end

            setProperty(propName, newVal)
            return true
        end
    end

    -- Slider click activation (sets widget as active for drag tracking)
    events.registerHandler("slider_activate", function(event, widget)
        if widget and widget.id then
            uiState.setActive(widget.id)
            return true
        end
        return false
    end)

    -- Linear slider: steps as fractions of range (default behavior)
    -- default=1%, ctrl=0.1%, shift=10%, ctrl+shift=25%
    events.registerHandler("slider_adjust", createSliderHandler(
        function(current, delta, minVal, maxVal, ctrl, shift)
            local range = maxVal - minVal
            local step
            if ctrl and shift then step = range * 0.25
            elseif shift then step = range * 0.10
            elseif ctrl then step = range * 0.001
            else step = range * 0.01 end
            return current + delta * step
        end,
        "slider_adjust"
    ))

    -- Logarithmic slider: multiplicative factors
    -- default=1.5x, ctrl=1.1x, shift=2x, ctrl+shift=5x
    events.registerHandler("slider_adjust_log", createSliderHandler(
        function(current, delta, minVal, maxVal, ctrl, shift)
            local factor
            if ctrl and shift then factor = 5
            elseif shift then factor = 2
            elseif ctrl then factor = 1.1
            else factor = 1.5 end
            if delta > 0 then return current * factor
            else return current / factor end
        end,
        "slider_adjust_log"
    ))

    -- Generic set value handler
    -- Uses SetBox properties: property, value
    events.registerHandler("set_value", function(event, widget, props)
        props = props or {}
        if props.property and props.value ~= nil then
            setProperty(props.property, props.value)
            return true
        end
        return false
    end)

    -- Generic toggle handler
    -- Uses SetBox properties: property
    events.registerHandler("toggle_property", function(event, widget, props)
        props = props or {}
        if props.property then
            local current = getProperty(props.property)
            if current ~= nil then
                setProperty(props.property, not current)
                return true
            end
        end
        return false
    end)

    -- =======================================================================
    -- Control Handlers (behavior-specific, not widget-specific)
    -- =======================================================================

    -- VFO Control handler - tunes frequency from any widget tagged VFOControl
    -- Knows: property="frequency", bounds=0.1-30.0 MHz
    -- Step comes from rule props (based on modifiers)
    events.registerHandler("vfo_control", function(event, widget, props)
        -- Determine delta from wheel or arrow keys
        local delta = event.delta
        if not delta then
            if event.key == "RIGHT" or event.key == "UP" then delta = 1
            elseif event.key == "LEFT" or event.key == "DOWN" then delta = -1
            else return false end
        end
        if delta == 0 then return false end

        -- Get step from rule (required)
        local step = props and props.step
        if not step then return false end

        -- Update frequency (AppState handles clamping and VFO/Band sync)
        setProperty("frequency", state.frequency + delta * step)

        if events.debugDispatch then
            print(string.format("[vfo_control] freq=%.6f step=%s delta=%d", state.frequency, step, delta))
        end

        return true
    end)

    -- =======================================================================
    -- Specific Event Handlers (for complex logic that can't be generalized)
    -- =======================================================================

    -- Start frequency entry mode (F key or click on frequency display)
    events.registerHandler("freq_entry_start", function(event, widget)
        if not events.hasModeTag("state.FreqEntryMode") then
            events.addModeTag("state.FreqEntryMode")
            freqEntryText = ""
            return true
        end
        return false
    end)

    -- Cancel frequency entry (ESC in FreqEntryMode)
    events.registerHandler("freq_entry_cancel", function(event, widget)
        events.removeModeTag("state.FreqEntryMode")
        freqEntryText = ""
        return true
    end)

    -- Confirm frequency entry (Enter in FreqEntryMode)
    events.registerHandler("freq_entry_confirm", function(event, widget)
        local newFreq = tonumber(freqEntryText)
        if newFreq then
            setProperty("frequency", newFreq)
        end
        events.removeModeTag("state.FreqEntryMode")
        freqEntryText = ""
        return true
    end)

    -- Backspace in frequency entry mode
    events.registerHandler("freq_entry_backspace", function(event, widget)
        if #freqEntryText > 0 then
            freqEntryText = freqEntryText:sub(1, -2)
        end
        return true
    end)

    -- Text input for frequency entry
    events.registerHandler("freq_entry_text", function(event, widget)
        if event.text then
            for i = 1, #event.text do
                local ch = event.text:sub(i, i)
                if ch:match("[0-9]") or (ch == "." and not freqEntryText:find("%.")) then
                    freqEntryText = freqEntryText .. ch
                end
            end
        end
        return true
    end)

    -- Quit application (Ctrl+Q)
    events.registerHandler("app_quit", function(event, widget)
        quit()
        return true
    end)

    -- Toggle event debug output (Ctrl+D)
    events.registerHandler("debug_toggle", function(event, widget)
        events.debugDispatch = not events.debugDispatch
        print(string.format("[Events] Debug output %s (Ctrl+D to toggle)",
            events.debugDispatch and "ENABLED" or "disabled"))
        return true
    end)

    -- Reset UI layout (Ctrl+Alt+R)
    events.registerHandler("layout_reset", function(event, widget)
        layoutOverrides.clear()
        layoutOverrides.save()
        print("[Edit] Layout overrides cleared and saved")
        return true
    end)

    -- Waterfall click to tune (placeholder for future)
    events.registerHandler("waterfall_click", function(event, widget)
        -- TODO: Calculate frequency offset from click position
        print(string.format("[Events] Waterfall click at (%d, %d)", event.x or 0, event.y or 0))
        return true
    end)

    -- =======================================================================
    -- VFO Button Handlers
    -- =======================================================================

    events.registerHandler("vfo_a_click", function(event, widget)
        setProperty("activeVFO", "A")
        return true
    end)

    events.registerHandler("vfo_b_click", function(event, widget)
        setProperty("activeVFO", "B")
        return true
    end)

    events.registerHandler("vfo_swap_click", function(event, widget)
        local a = getProperty("vfoA")
        local b = getProperty("vfoB")
        AppState.batch(function()
            setProperty("vfoA", b)
            setProperty("vfoB", a)
        end)
        return true
    end)

    -- =======================================================================
    -- Audio Control Handlers
    -- =======================================================================

    events.registerHandler("wav_record_toggle", function(event, widget)
        local isRecording = audio.isRecording()
        if isRecording then
            audio.stopRecording()
        else
            audio.startRecording("/tmp/nexrx_audio.wav")
        end
        return true
    end)

    -- =======================================================================
    -- RX Toggle Handler
    -- =======================================================================

    events.registerHandler("rx_toggle_click", function(event, widget)
        state.rxActive = not state.rxActive
        return true
    end)

    -- Generic no-op handler for MouseUp events on interactive widgets
    events.registerHandler("noop", function(event, widget)
        return true  -- Mark as handled, do nothing
    end)

    -- Enhanced unhandled event logger with full details
    -- Motion events are silent (too frequent, expected to be unhandled most of the time)
    events.registerHandler("log_unhandled", function(event, widget)
        -- Skip motion events to avoid spam
        if event.type == events.Type.MOUSE_MOVE then
            return false
        end

        local parts = {event.type or "?"}

        if event.button then table.insert(parts, "button=" .. event.button) end
        if event.key then table.insert(parts, "key=" .. event.key) end
        if event.x then table.insert(parts, string.format("@%d,%d", event.x, event.y or 0)) end
        if event.delta then table.insert(parts, "delta=" .. event.delta) end

        local mods = event.modifiers and #event.modifiers > 0
            and "[" .. table.concat(event.modifiers, "+") .. "]" or ""
        if mods ~= "" then table.insert(parts, mods) end

        local wid = widget and widget.id or "none"
        table.insert(parts, "widget=" .. wid)

        -- Show widget tags for debugging handler matching
        if widget and widget.tags then
            local tagStr = table.concat(widget.tags, ",")
            table.insert(parts, "tags={" .. tagStr .. "}")
        end

        print("[Events] UNHANDLED: " .. table.concat(parts, " "))
        return false  -- Not handled, but logged
    end)

    -- Initialize dispatch module
    dispatch.init()

    -- Enable hardware dispatch if connected
    if hwConnected then
        dispatch.enableHardware()
    end

    -- Enable waterfall dispatch if initialized
    if waterfall.isInitialized() then
        dispatch.enableWaterfall()
    end

    -- Initialize animation system (nothing to configure, just ready to use)
    print("[Lua] Animation system ready")

    -- Initialize reactive state system
    AppState.init()

    -- Initialize editing handlers (Ctrl+Alt+drag to resize/move)
    Edit.init(events)

    print("[Lua] NexRx UI initialized with layout system + reactive properties")
    print("[Lua] Ctrl+Alt+Left drag edge to resize, Ctrl+Alt+Middle drag to move")
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

    -- Update animation system
    animate.update(dt)

    -- Frequency entry mode handling (blink cursor)
    -- freqEntryMode is now managed via events.hasModeTag("state.FreqEntryMode")
    freqEntryBlink = freqEntryBlink + dt
    if freqEntryBlink > 1.0 then freqEntryBlink = 0 end

    -- =======================================================================
    -- Input State Synchronization
    -- =======================================================================

    -- 1. Sync keyboard state into activeTags
    for _, scancode in ipairs(keys.getAllScancodes()) do
        local isDown = isKeyDown(scancode)
        local keyName = keys.getName(scancode)
        local inputTag = keyName and ("input." .. keyName) or nil
        if isDown then
            if inputTag then activeTags[inputTag] = true end
        else
            if inputTag then activeTags[inputTag] = nil end
        end
    end

    -- Ensure standard modifier tags are present if any of their specific keys are down
    if isShiftDown and isShiftDown() then activeTags["input.SHIFT"] = true end
    if isCtrlDown and isCtrlDown() then activeTags["input.CTRL"] = true end
    if isAltDown and isAltDown() then activeTags["input.ALT"] = true end

    -- 2. Sync mouse button state into activeTags
    activeTags["input.MouseLEFT"] = isMouseDown(0) or nil
    activeTags["input.MouseMIDDLE"] = isMouseDown(1) or nil
    activeTags["input.MouseRIGHT"] = isMouseDown(2) or nil

    -- =======================================================================
    -- Event dispatch via SetBox
    -- =======================================================================

    local mouseX, mouseY = getMousePos()

    -- 3. Return all currently active tags (held keys, held buttons, mode tags)
    -- All tags use namespaced format: input.SHIFT, input.CTRL, etc.
    local function getActiveTags()
        local tags = {}
        for tagName, _ in pairs(activeTags) do
            table.insert(tags, tagName)
        end
        -- Derive generic modifier tags from specific keys
        -- (allows rules to match "input.SHIFT" for either input.LSHIFT or input.RSHIFT)
        if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then
            table.insert(tags, "input.SHIFT")
        end
        if activeTags["input.LCTRL"] or activeTags["input.RCTRL"] then
            table.insert(tags, "input.CTRL")
        end
        if activeTags["input.LALT"] or activeTags["input.RALT"] then
            table.insert(tags, "input.ALT")
        end

        -- Add mode tags from events module
        if events and events.getModeTags then
            for _, modeTag in ipairs(events.getModeTags()) do
                table.insert(tags, modeTag)
            end
        end

        return tags
    end

    -- Booleans for handlers that still use them (legacy, prefer tags)
    local shift = activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"]
    local ctrl = activeTags["input.LCTRL"] or activeTags["input.RCTRL"]
    local alt = activeTags["input.LALT"] or activeTags["input.RALT"]

    -- Dispatch mouse wheel events
    local wheel = getMouseWheel()
    if wheel ~= 0 then
        local wheelEvent = events.createEvent(events.Type.MOUSE_WHEEL, {
            x = mouseX,
            y = mouseY,
            delta = wheel,
            modifiers = getActiveTags(),
            shift = shift,
            ctrl = ctrl,
            alt = alt,
        })
        events.dispatch(wheelEvent)
    end

    -- Dispatch mouse button events (all buttons)
    -- Track held buttons in activeTags (namespaced as input.MouseLEFT, etc.)
    for btn = 0, 2 do
        local button = BUTTON_NAMES[btn + 1]  -- LEFT, MIDDLE, RIGHT
        local inputTag = "input.Mouse" .. button  -- input.MouseLEFT, etc.
        local clicked = isMouseClicked(btn)
        local released = isMouseReleased(btn)

        if clicked then
            -- Button just pressed - add to active tags and dispatch MouseDown
            activeTags[inputTag] = true
            events.dispatch(events.createEvent(events.Type.MOUSE_DOWN, {
                x = mouseX, y = mouseY, button = button,
                modifiers = getActiveTags(),
                shift = shift, ctrl = ctrl, alt = alt,
            }))
        elseif released then
            -- Button just released - remove from active tags and dispatch MouseUp
            activeTags[inputTag] = nil
            events.dispatch(events.createEvent(events.Type.MOUSE_UP, {
                x = mouseX, y = mouseY, button = button,
                modifiers = getActiveTags(),
                shift = shift, ctrl = ctrl, alt = alt,
            }))
        end
    end

    -- Dispatch mouse motion events (activeTags already includes held buttons)
    if mouseX ~= lastMouseX or mouseY ~= lastMouseY then
        events.dispatch(events.createEvent(events.Type.MOUSE_MOVE, {
            x = mouseX,
            y = mouseY,
            dx = mouseX - lastMouseX,
            dy = mouseY - lastMouseY,
            modifiers = getActiveTags(),
            shift = shift,
            ctrl = ctrl,
            alt = alt,
        }))
        lastMouseX, lastMouseY = mouseX, mouseY
    end

    -- Dispatch KeyDown/KeyUp events
    for _, scancode in ipairs(keys.getAllScancodes()) do
        local isDown = isKeyDown(scancode)
        local wasDown = keyStates[scancode]
        local keyName = keys.getName(scancode)

        if isDown and not wasDown then
            events.dispatchKey(events.createEvent(events.Type.KEY_DOWN, {
                key = keyName, scancode = scancode,
                char = keys.getChar(scancode),
                isModifier = keys.isModifier(scancode),
                x = mouseX, y = mouseY,
                modifiers = getActiveTags(),
                shift = shift, ctrl = ctrl, alt = alt,
            }))
        elseif not isDown and wasDown then
            events.dispatchKey(events.createEvent(events.Type.KEY_UP, {
                key = keyName, scancode = scancode,
                char = keys.getChar(scancode),
                isModifier = keys.isModifier(scancode),
                x = mouseX, y = mouseY,
                modifiers = getActiveTags(),
                shift = shift, ctrl = ctrl, alt = alt,
            }))
        end
        keyStates[scancode] = isDown
    end

    -- Dispatch text input (steering handled via SetBox rules)
    local textIn = getTextInput()
    if #textIn > 0 then
        events.dispatchKey(events.createEvent(events.Type.TEXT_INPUT, {
            text = textIn,
            x = mouseX, y = mouseY,
            modifiers = getActiveTags(),
            shift = shift, ctrl = ctrl, alt = alt,
        }))
    end

    -- Get spectrum data using dispatch module (eliminates conditionals)
    -- dispatch.getSpectrum() returns hardware data or nil, fallback to simulated
    local hwSpectrum = dispatch.getSpectrum()
    if hwSpectrum then
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
        hwFramesReceived = hw.getFramesReceived()
        hwConnected = true
    else
        -- Fall back to simulated spectrum
        generateSpectrum()
        hwConnected = hw.isConnected()
    end

    -- Update waterfall using dispatch (no-op if not initialized)
    dispatch.updateWaterfall(spectrumData)
end

-- =======================================================================
-- UI Helper Functions
-- Reduce boilerplate for common UI patterns
-- =======================================================================

-- Draw a labeled slider that reads/writes from property system
-- Uses AppState specs for min/max bounds, AppState for get/set
local function drawPropertySlider(id, label, prop, w, fmt)
    local spec = AppState.getSpec(prop)
    local lx, ly = layout.getCursor()
    ui.label(id .. "-label", lx, ly, label .. ":")
    local minVal = spec and spec.min or 0
    local maxVal = spec and spec.max or 1
    local current = getProperty(prop) or 0
    local tags = spec and spec.logScale and {"LogScale"} or nil
    local newVal = ui.slider(id, lx + 55, ly, w - 85, minVal, maxVal, current, tags, prop)
    if newVal ~= current then
        setProperty(prop, newVal)
    end
    local text = string.format(fmt or "%.0f", current)
    drawText(lx + w - 70, ly - 2, text, 0.5, 0.5, 0.55, 1.0)
    layout.newLine(24)
end

-- Draw center/width filter controls (used for bandpass and notch)
local function drawFilterControls(prefix, enabledProp, centerProp, widthProp, w)
    if not state[enabledProp] then return end
    drawPropertySlider(prefix .. "-center", "Center", centerProp, w, "%.0f Hz")
    drawPropertySlider(prefix .. "-width", "Width", widthProp, w, "%.0f Hz")
end

-- Query layout size from SetBox constraints given widget tags and optional widgetId
-- Checks layout_overrides first, then falls back to SetBox constraints
-- Returns evaluated size (width or height) with min/max applied
local function getLayoutSize(tags, sizeKey, parent, widgetId)
    -- Check overrides first
    if widgetId then
        local override = layoutOverrides.get(widgetId, sizeKey)
        if override then return override end
    end

    local cons = constraints.query(tags)
    if not cons[sizeKey] then return nil end

    local ctx = {
        parent = parent,
        window = {width = parent.width, height = parent.height},
        math = math,
    }

    -- Evaluate the size expression
    local expr = cons[sizeKey]
    local size
    if type(expr) == "number" then
        size = expr
    else
        local num = tonumber(expr)
        if num then
            size = num
        else
            local fn = load("return " .. expr, "constraint", "t", ctx)
            if fn then
                local ok, result = pcall(fn)
                size = ok and result or nil
            end
        end
    end

    if not size then return nil end

    -- Apply min/max
    local minKey = sizeKey == "width" and "minWidth" or "minHeight"
    local maxKey = sizeKey == "width" and "maxWidth" or "maxHeight"
    local minVal = cons[minKey] and tonumber(cons[minKey])
    local maxVal = cons[maxKey] and tonumber(cons[maxKey])
    if minVal then size = math.max(minVal, size) end
    if maxVal then size = math.min(maxVal, size) end

    return size
end

-- Draw
function draw()
    local winW, winH = getWindowSize()
    local mouseX, mouseY = getMousePos()

    -- Clear widget registry at start of draw (keeps previous frame's widgets
    -- available during update() for event dispatch, then rebuild here)
    events.clearWidgets()

    ui.beginFrame()
    layout.begin(0, 0, winW, winH)

    -- Solve all widget regions at once
    local regions = container.solve(winW, winH)

    -- =========================================
    -- TOP BAR - Status and branding
    -- =========================================
    do
        local r = regions["top-bar"]
        local x, y, w, h = r.x, r.y, r.w, r.h
        layout.setRegion(x, y, w, h, "top-bar")

        -- Register top bar for event dispatch (ID must match container.lua's widgetOrder)
        events.registerWidget("top-bar", {x=x, y=y, w=w, h=h}, {"widget.StatusBar", "widget.UIPanel"}, nil)

        drawRect(x, y, w, h, 0.08, 0.08, 0.12, 1.0)
        drawLine(x, y + h, x + w, y + h, 0.2, 0.2, 0.25, 1.0, 1.0)

        -- Branding
        local brandX = x + 10
        drawText(brandX, y + 8, "NexRx SDR Receiver", 0.6, 0.7, 0.9, 1.0)
        local brandW = measureText("NexRx SDR Receiver")

        -- Hardware connection status
        local hwStatus = hwConnected and "HW" or "SIM"
        local hwStatusColor = hwConnected and {0.2, 0.9, 0.4} or {0.6, 0.6, 0.6}
        local hwX = brandX + brandW + 30
        drawText(hwX, y + 8, hwStatus, hwStatusColor[1], hwStatusColor[2], hwStatusColor[3], 1.0)
        local hwStatusW = measureText(hwStatus)
        
        if hwConnected then
            local framesText = string.format(" (%d frames)", hwFramesReceived)
            drawText(hwX + hwStatusW + 4, y + 8, framesText, 0.5, 0.5, 0.55, 1.0)
            local framesW = measureText(framesText)

            -- Audio stats for debugging (now includes drops and fill ratio)
            local audioWritten, audioRead, underruns, drops, fillRatio = rx.getAudioStats()
            local audioText = string.format("Buf: %.0f%%", fillRatio * 100)
            local audioX = hwX + hwStatusW + framesW + 40
            drawText(audioX, y + 8, audioText, 0.5, 0.7, 0.5, 1.0)
            local audioW = measureText(audioText)

            -- Drop rates (IQ and Audio)
            local iqDropRate = hw.getIqDropRate()
            local audioDropRate = rx.getAudioDropRate()
            local dropText = string.format("Drop: IQ=%.0f/s A=%.0f/s", iqDropRate, audioDropRate)
            -- Color red if drops > 0
            local dropR = (iqDropRate > 0 or audioDropRate > 0) and 1.0 or 0.5
            local dropG = (iqDropRate > 0 or audioDropRate > 0) and 0.3 or 0.7
            local dropX = audioX + audioW + 40
            drawText(dropX, y + 8, dropText, dropR, dropG, 0.5, 1.0)
        end

        -- Status (Mouse and FPS) - Align to right
        local statusText = string.format("Mouse: %d, %d | FPS: %.0f", mouseX, mouseY, fps)
        local statusW = measureText(statusText)
        drawText(x + w - statusW - 10, y + 8, statusText, 0.5, 0.5, 0.55, 1.0)
        layout.endRegion()
    end

    -- =========================================
    -- LEFT SIDEBAR - Controls
    -- =========================================
    do
        local r = regions["left-sidebar"]
        local x, y, w, h = r.x, r.y, r.w, r.h
        layout.setRegion(x, y, w, h, "left-sidebar")
        ui.panel("left-sidebar", x, y, w, h, {"Sidebar"})
        layout.pad(12)

        -- VFO Section
        local lx, ly = layout.getCursor()
        ui.label("vfo-title", lx, ly, "VFO", {"Title"})
        layout.newLine(20)

        layout.beginHorizontal()
        local vfoATags = state.activeVFO == "A" and {"Primary", "VfoA"} or {"Secondary", "VfoA"}
        local vfoBTags = state.activeVFO == "B" and {"Primary", "VfoB"} or {"Secondary", "VfoB"}
        local bx, by = layout.reserveSpace(60, 28)
        ui.button("vfo-a", "VFO A", bx, by, 60, 28, vfoATags)
        bx, by = layout.reserveSpace(60, 28)
        ui.button("vfo-b", "VFO B", bx, by, 60, 28, vfoBTags)
        bx, by = layout.reserveSpace(50, 28)
        ui.button("vfo-swap", "A<>B", bx, by, 50, 28, {"VfoSwap"})
        layout.endHorizontal()

        layout.space(8)

        -- Frequency display (clickable/scrollable for tuning)
        local fx, fy = layout.getCursor()
        ui.frequencyDisplay("sidebar-freq-display", fx, fy, w - 24, 36, state.frequency, freqEntryText)
        layout.newLine(40)

        -- Frequency slider (wheel/arrow handled via VFOControl rules in events.lua)
        local sx, sy = layout.getCursor()
        local newFreq = ui.slider("freq-slider", sx, sy, w - 24, 1.0, 30.0, state.frequency, {"VFOControl"}, "frequency")
        if newFreq ~= state.frequency then
            state.frequency = newFreq
        end
        layout.newLine(20)

        layout.space(8)
        local sepX, sepY = layout.getCursor()
        ui.separator("sep-vfo-mode", sepX, sepY, w - 24)
        layout.newLine(8)

        -- Mode Selection
        lx, ly = layout.getCursor()
        ui.label("mode-title", lx, ly, "Mode", {"Title"})
        layout.newLine(24)

        layout.beginHorizontal(4)
        for _, mode in ipairs(modeHelper.names) do
            local mx, my = layout.reserveSpace(50, 26)
            local activeTags = state.selectedMode == mode and {"Active"} or {}
            local modeTags = {"Mode" .. mode}
            for _, t in ipairs(activeTags) do table.insert(modeTags, t) end
            ui.button("mode-" .. mode:lower(), mode, mx, my, 50, 26, modeTags)
        end
        layout.endHorizontal()

        layout.newLine(12)

        -- Band Selection
        lx, ly = layout.getCursor()
        ui.label("band-title", lx, ly, "Band", {"Title"})
        layout.newLine(24)

        layout.beginHorizontal(4)
        local bandList = {"160m", "80m", "40m", "20m", "15m", "10m"}
        for _, bandName in ipairs(bandList) do
            local bx, by = layout.reserveSpace(40, 26)
            local activeTags = state.selectedBand == bandName and {"Active"} or {}
            local bandTags = {"Band" .. (bandName:gsub("m", "")) .. "m"} -- Ensure Band40m format
            for _, t in ipairs(activeTags) do table.insert(bandTags, t) end
            ui.button("band-" .. bandName, bandName, bx, by, 40, 26, bandTags)
        end
        layout.endHorizontal()


        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator("sep-mode-baseband", sepX, sepY, w - 24)
        layout.newLine(8)

        -- Baseband Filter (FIR)
        lx, ly = layout.getCursor()
        ui.label("baseband-title", lx, ly, "Baseband", {"Title"})
        layout.newLine(24)

        -- Bandpass enable + controls
        local cx, cy = layout.getCursor()
        ui.checkbox("bp-enable", "Bandpass", cx, cy, state.bandpassEnabled, {"Bandpass"})
        layout.newLine(24)
        drawFilterControls("bp", "bandpassEnabled", "bandpassCenter", "bandpassWidth", w)

        -- Notch enable + controls
        cx, cy = layout.getCursor()
        ui.checkbox("notch-enable", "Notch", cx, cy, state.notchEnabled, {"Notch"})
        layout.newLine(24)
        drawFilterControls("notch", "notchEnabled", "notchCenter", "notchWidth", w)

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator("sep-baseband-dsp", sepX, sepY, w - 24)
        layout.newLine(8)

        -- DSP Options
        lx, ly = layout.getCursor()
        ui.label("dsp-title", lx, ly, "DSP", {"Title"})
        layout.newLine(24)

        local cx, cy = layout.getCursor()
        ui.checkbox("agc-enable", "AGC", cx, cy, state.agcEnabled, {"AGC"})
        layout.newLine(24)

        cx, cy = layout.getCursor()
        ui.checkbox("nr-enable", "Noise Reduction", cx, cy, state.nrEnabled, {"NR"})
        layout.newLine(24)

        cx, cy = layout.getCursor()
        ui.checkbox("nb-enable", "Noise Blanker", cx, cy, state.nbEnabled, {"NB"})
        layout.newLine(28)

        -- QSD offset k for image rejection
        lx, ly = layout.getCursor()
        ui.label("qsd-k-label", lx, ly, "QSD k:")
        local newK = ui.slider("qsd-k", lx + 50, ly, w - 80, 1, 24, state.qsdOffsetK, nil, "qsdOffsetK")
        if newK ~= state.qsdOffsetK then
            state.qsdOffsetK = newK
            hw.setQsdOffset(state.qsdOffsetK)
        end
        local kText = string.format("%.1f kHz", state.qsdOffsetK)
        drawText(lx + w - 70, ly - 2, kText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        -- LMS mu (learning rate) for image rejection
        -- Logarithmic scale: slider 0-1 maps to mu 0.0001 to 0.1
        lx, ly = layout.getCursor()
        ui.label("lms-mu-label", lx, ly, "LMS:")
        local function muToSlider(mu)
            -- Map 0.0001..0.1 to 0..1 (log scale)
            return (math.log(mu) - math.log(0.0001)) / (math.log(0.1) - math.log(0.0001))
        end
        local function sliderToMu(pos)
            -- Map 0..1 to 0.0001..0.1 (log scale)
            local logMu = math.log(0.0001) + pos * (math.log(0.1) - math.log(0.0001))
            return math.exp(logMu)
        end
        local muSliderPos = muToSlider(state.lmsMu)
        local newMuSliderPos = ui.slider("lms-mu", lx + 40, ly, w - 70, 0, 1, muSliderPos, {"LogScale"}, "lmsMu")
        if newMuSliderPos ~= muSliderPos then
            state.lmsMu = sliderToMu(newMuSliderPos)
            rx.setLmsMu(state.lmsMu)
        end
        local muText = string.format("%.4f", state.lmsMu)
        drawText(lx + w - 60, ly - 2, muText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        -- RF Attenuator (0-45 dB in 3 dB steps)
        lx, ly = layout.getCursor()
        ui.label("rf-atten-label", lx, ly, "Atten:")
        -- Slider goes 0-15 (steps), displayed as 0-45 dB
        local attenSteps = state.rfAttenDb / 3
        local newAttenSteps = ui.slider("rf-atten", lx + 50, ly, w - 80, 0, 15, attenSteps, nil, "rfAttenDb")
        local newAttenDb = math.floor(newAttenSteps + 0.5) * 3  -- Round to nearest 3 dB
        if newAttenDb ~= state.rfAttenDb then
            state.rfAttenDb = newAttenDb
            hw.setAttenuation(state.rfAttenDb)
        end
        local attenText = string.format("%d dB", state.rfAttenDb)
        drawText(lx + w - 60, ly - 2, attenText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator("sep-dsp-audio", sepX, sepY, w - 24)
        layout.newLine(8)

        -- Volume & Squelch
        lx, ly = layout.getCursor()
        ui.label("audio-title", lx, ly, "Audio", {"Title"})
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label("volume-label", lx, ly, "Vol:")
        -- Volume slider works directly in dB (-60 to 0)
        local newVolumeDb = ui.slider("volume", lx + 40, ly, w - 70, -60, 0, state.volumeDb, nil, "volumeDb")
        if newVolumeDb ~= state.volumeDb then
            state.volumeDb = newVolumeDb
            local linearGain = math.pow(10, state.volumeDb / 20)
            audio.setVolume(linearGain)
        end
        local volText = string.format("%.0fdB", state.volumeDb)
        drawText(lx + w - 60, ly - 2, volText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label("squelch-label", lx, ly, "Sql:")
        local newSquelch = ui.slider("squelch", lx + 40, ly, w - 70, 0, 1, state.squelch, nil, "squelch")
        if newSquelch ~= state.squelch then state.squelch = newSquelch end
        layout.newLine(24)

        -- Mute checkbox
        cx, cy = layout.getCursor()
        ui.checkbox("mute-enable", "Mute", cx, cy, state.muteEnabled, {"Mute"})
        layout.newLine(24)

        -- Test tone button
        local toneTags = state.testToneEnabled and {"Primary", "TestTone"} or {"Secondary", "TestTone"}
        local toneLabel = state.testToneEnabled and "Tone ON" or "Test Tone"
        local tx, ty = layout.getCursor()
        ui.button("test-tone", toneLabel, tx, ty, 90, 26, toneTags)
        layout.newLine(28)

        -- WAV Recording button
        local isRecording = audio.isRecording()
        local recTags = isRecording and {"Danger", "WavRecord"} or {"Secondary", "WavRecord"}
        local recLabel = isRecording and "REC" or "Record"
        local recBtnX, recBtnY = layout.getCursor()
        ui.button("wav-record", recLabel, recBtnX, recBtnY, 70, 26, recTags)
        -- Show recording duration
        if isRecording then
            local duration = audio.getRecordingDuration()
            local durText = string.format("%.1fs", duration)
            drawText(recBtnX + 78, recBtnY + 6, durText, 1.0, 0.3, 0.3, 1.0)
        end
        layout.newLine(28)
        layout.endRegion()
    end

    -- =========================================
    -- RIGHT SIDEBAR - S-Meter and Band
    -- =========================================
    do
        local r = regions["right-sidebar"]
        local x, y, w, h = r.x, r.y, r.w, r.h
        layout.setRegion(x, y, w, h, "right-sidebar")
        ui.panel("right-sidebar", x, y, w, h, {"Sidebar"})
        layout.pad(12)

        -- RX Toggle (styling controlled by SetBox rules based on Checked tag)
        local toggleX, toggleY = layout.getCursor()
        local rxLabel = state.rxActive and "RX ON" or "RX OFF"
        ui.toggle("rx-toggle", rxLabel, toggleX, toggleY, w - 24, 40, state.rxActive, {"RxToggle"})
        layout.newLine(52)

        local sepX, sepY = layout.getCursor()
        ui.separator("sep-rx-smeter", sepX, sepY, w - 24)
        layout.newLine(12)

        -- S-Meter
        local lx, ly = layout.getCursor()
        ui.label("smeter-title", lx, ly, "S-Meter", {"Title"})
        layout.newLine(24)

        local mx, my = layout.getCursor()
        local sig = smeter.getReading()
        local consumedH = ui.smeter("s-meter", mx, my, w - 24, 28, sig)
        layout.newLine(consumedH + 12)

        local sepX, sepY = layout.getCursor()
        ui.separator("sep-smeter-display", sepX, sepY, w - 24)
        layout.newLine(12)

        -- Waterfall Range
        lx, ly = layout.getCursor()
        ui.label("display-title", lx, ly, "Display", {"Title"})
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label("wf-floor-label", lx, ly, "Floor:")
        local newMinDb = ui.slider("wf-floor", lx + 45, ly, w - 75, -140, -60, state.wfMinDb, nil, "wfMinDb")
        if newMinDb ~= state.wfMinDb then
            state.wfMinDb = newMinDb
            waterfall.setRange(state.wfMinDb, state.wfMaxDb)
        end
        local minText = string.format("%.0f dB", state.wfMinDb)
        drawText(lx + w - 60, ly - 2, minText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)

        lx, ly = layout.getCursor()
        ui.label("wf-peak-label", lx, ly, "Peak:")
        local newMaxDb = ui.slider("wf-peak", lx + 45, ly, w - 75, -100, -20, state.wfMaxDb, nil, "wfMaxDb")
        if newMaxDb ~= state.wfMaxDb then
            state.wfMaxDb = newMaxDb
            waterfall.setRange(state.wfMinDb, state.wfMaxDb)
        end
        local maxText = string.format("%.0f dB", state.wfMaxDb)
        drawText(lx + w - 60, ly - 2, maxText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(24)
        layout.endRegion()
    end

    -- =========================================
    -- DEBUG PANEL - Active Tags Viewer (far right)
    -- =========================================
    do
        local r = regions["active-tags"]
        local x, y, w, h = r.x, r.y, r.w, r.h
        layout.setRegion(x, y, w, h, "active-tags")
        -- Get ALL active tags (held keys, derived modifiers, mode tags)
        local allTags = getAllActiveTags()
        ui.activeTagsViewer("active-tags", x + 8, y + 8, w - 16, h - 16, allTags)
        layout.endRegion()
    end

    -- =========================================
    -- CENTER - Spectrum and Waterfall Display
    -- =========================================
    do
        local rCenter = regions["center-area"]
        ui.panel("center-panel", rCenter.x, rCenter.y, rCenter.w, rCenter.h)
        
        -- Solve sub-layout for spectrum and waterfall
        local subRegions = container.solveSublayout(rCenter, {
            { id = "spectrum-display",  tags = {"widget.Spectrum"},  anchor = "top", group = "center-col" },
            { id = "waterfall-display", tags = {"widget.Waterfall"}, anchor = "top", group = "center-col" },
        })

        -- Spectrum Display
        local rSpec = subRegions["spectrum-display"]
        layout.setRegion(rSpec.x, rSpec.y, rSpec.w, rSpec.h, "spectrum-display")
        dispatch.renderSpectrum(spectrumData, rSpec.x, rSpec.y, rSpec.w, rSpec.h)
        events.registerWidget("spectrum-display",
            {x = rSpec.x, y = rSpec.y, w = rSpec.w, h = rSpec.h},
            {"widget.Spectrum", "widget.VFOControl"},
            nil)
        layout.endRegion()

        -- Waterfall Display
        local rWf = subRegions["waterfall-display"]
        layout.setRegion(rWf.x, rWf.y, rWf.w, rWf.h, "waterfall-display")
        dispatch.renderWaterfall(rWf.x, rWf.y, rWf.w, rWf.h)
        events.registerWidget("waterfall-display",
            {x = rWf.x, y = rWf.y, w = rWf.w, h = rWf.h},
            {"widget.Waterfall", "widget.VFOControl"},
            nil)
        layout.endRegion()

        -- Center frequency marker
        local centerX = rSpec.x + rSpec.w / 2
        drawLine(centerX, rSpec.y, centerX, rWf.y + rWf.h, 0.9, 0.3, 0.3, 0.5, 1.0)
    end

    -- =========================================
    -- EDIT HANDLES - Draw when Ctrl+Alt held
    -- =========================================
    local allTags = getAllActiveTags()
    local editModifierHeld = Edit.isEditModifierHeld(allTags)
    if editModifierHeld then
        local hoveredWidget = events.getWidgetAt(mouseX, mouseY)
        Edit.drawHandles(mouseX, mouseY, hoveredWidget, editModifierHeld)
    end

    layout.finish()
    ui.endFrame()
end

print("[Lua] main.lua loaded")
