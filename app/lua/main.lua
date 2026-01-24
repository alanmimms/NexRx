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
local bands = require("bands")
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

-- Application state (all loaded from SetBox in init())
local rxActive = false

-- =======================================================================
-- Property-controlled state
-- All values accessible via getProperty/setProperty using state[name]
-- Specs define min/max/step bounds and C++ setter functions
-- =======================================================================

local state = {}  -- Populated in init() from SetBox

-- Property specifications: defines behavior for each property
-- Fields: min, max (bounds), step (quantization), setter (C++ function), requiresHw (conditional)
local propertySpecs = {
    -- Boolean toggles (no bounds, just setter)
    bandpassEnabled = { setter = function(v) rx.setBandpassEnabled(v) end },
    notchEnabled    = { setter = function(v) rx.setNotchEnabled(v) end },
    agcEnabled      = { setter = function(v) rx.setAgcEnabled(v) end },
    nrEnabled       = { setter = function(v) rx.setNrEnabled(v) end },
    nbEnabled       = { setter = function(v) rx.setNbEnabled(v) end },
    muteEnabled     = { setter = function(v) rx.setMute(v) end },
    testToneEnabled = { setter = function(v) audio.setTestTone(v, 440.0) end },

    -- Numeric with bounds
    bandpassCenter = { min = -5000, max = 5000, setter = function(v) rx.setBandpassCenter(v) end },
    bandpassWidth  = { min = 50, max = 4000, setter = function(v) rx.setBandpassWidth(v) end },
    notchCenter    = { min = -10000, max = 10000, setter = function(v) rx.setNotchCenter(v) end },
    notchWidth     = { min = 10, max = 500, setter = function(v) rx.setNotchWidth(v) end },
    volumeDb       = { min = -60, max = 0, setter = function(v) audio.setVolume(v) end },
    squelch        = { min = 0, max = 1 },  -- No C++ setter
    lmsMu          = { min = 0.00001, max = 0.1, setter = function(v) rx.setLmsMu(v) end },

    -- Waterfall range (setters reference other state values)
    wfMinDb = { min = -140, max = -60, setter = function(v) waterfall.setRange(v, state.wfMaxDb) end },
    wfMaxDb = { min = -100, max = -20, setter = function(v) waterfall.setRange(state.wfMinDb, v) end },

    -- Hardware-conditional (only call setter if hw connected)
    qsdOffsetK = { min = 1, max = 24, setter = function(v) hw.setQsdOffset(v) end, requiresHw = true },
    rfAttenDb  = { min = 0, max = 45, step = 3, setter = function(v) hw.setAttenuation(v) end, requiresHw = true },

    -- Special properties (custom logic in propertyAccessors, not auto-generated)
    -- frequency, selectedMode, selectedBand, wfColormap, vfoA, vfoB, activeVFO
}

-- VFO state (also in state table but not auto-generated)
-- state.vfoA, state.vfoB, state.activeVFO, state.frequency, state.selectedMode, state.selectedBand, state.wfColormap

-- Non-property state (not exposed via property system)
local wfBins           -- FFT bins (read-only after init)
local wfRows           -- history rows (read-only after init)
local spectrumData = {}  -- runtime buffer

-- Key state tracking for KeyUp detection
local keyStates = {}     -- scancode -> true if key was down last frame
local activeTags = {}    -- tag name -> true for all currently active tags (held keys, held buttons, etc.)

-- Property accessor registry (populated in init(), used by drawPropertySlider)
local propertyAccessors = {}

-- Get/set property values (works after init() populates propertyAccessors)
local function getProperty(name)
    local accessor = propertyAccessors[name]
    if accessor then return accessor.get() end
    return nil
end

local function setProperty(name, value)
    local accessor = propertyAccessors[name]
    if accessor then accessor.set(value) end
end

-- Get ALL active tags for debugging - EVERYTHING in the system
-- Shows complete tag state: all widgets, all inputs, all modes
-- All tags use unified namespaces: input.*, state.*, widget.*, event.*
local function getAllActiveTags()
    local allTags = {}

    -- 1. Raw held keys and mouse buttons (already namespaced as input.*)
    for tagName, _ in pairs(activeTags) do
        allTags[tagName] = true
    end

    -- 2. Derived generic modifiers (input.SHIFT from input.LSHIFT/input.RSHIFT, etc.)
    if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then
        allTags["input.SHIFT"] = true
    end
    if activeTags["input.LCTRL"] or activeTags["input.RCTRL"] then
        allTags["input.CTRL"] = true
    end
    if activeTags["input.LALT"] or activeTags["input.RALT"] then
        allTags["input.ALT"] = true
    end

    -- 3. Mode tags from events module (state.FreqEntryMode, etc.)
    if events and events.modeTags then
        for tagName, _ in pairs(events.modeTags) do
            allTags[tagName] = true
        end
    end

    -- 4. ALL registered widget tags (every widget in the UI)
    if events and events.widgets then
        for widgetId, widget in pairs(events.widgets) do
            if widget.tags then
                for _, tag in ipairs(widget.tags) do
                    allTags[tag] = true
                end
            end
        end
    end

    -- 5. Hovered widget state
    local mouseX, mouseY = getMousePos()
    local hoveredWidget = events.getWidgetAt(mouseX, mouseY)
    if hoveredWidget then
        allTags["state.Hovered"] = true
        -- Show which widget is hovered
        if hoveredWidget.id then
            allTags["state.Hovered:" .. hoveredWidget.id] = true
        end
    end

    -- 6. Pressed state (if mouse button held)
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
        -- Send initial QSD offset k
        hw.setQsdOffset(state.qsdOffsetK)
        print("[Lua] Set QSD offset k = " .. state.qsdOffsetK .. " kHz")
        -- Enable hardware dispatch functions
        dispatch.enableHardware()
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

    -- ==========================================================================
    -- Load all configuration from SetBox into state table
    -- ==========================================================================

    -- Radio settings
    state.frequency = setbox.getNumber("defaultFrequency", 14.200e6) / 1e6  -- Convert Hz to MHz
    state.selectedMode = setbox.getString("defaultMode", "USB")
    state.selectedBand = setbox.getString("defaultBand", "20m")

    -- VFO state
    state.vfoA = setbox.getNumber("vfoA", 14.200e6) / 1e6  -- Convert Hz to MHz
    state.vfoB = setbox.getNumber("vfoB", 7.050e6) / 1e6   -- Convert Hz to MHz
    state.activeVFO = setbox.getString("activeVFO", "A")

    -- DSP settings
    state.squelch = setbox.getNumber("squelch", 0.3)
    state.agcEnabled = setbox.getBool("agcEnabled", true)
    state.nrEnabled = setbox.getBool("nrEnabled", false)
    state.nbEnabled = setbox.getBool("nbEnabled", false)
    state.lmsMu = setbox.getNumber("lmsMu", 0.001)

    -- Audio settings (volume in dB for intuitive config)
    state.volumeDb = setbox.getNumber("volumeDb", -20)
    state.muteEnabled = setbox.getBool("muted", false)
    state.testToneEnabled = false  -- Always start with test tone off

    -- Baseband filter settings (offset from SetBox, or defaults)
    state.bandpassEnabled = setbox.getBool("bandpassEnabled", false)
    state.bandpassCenter = setbox.getNumber("bandpassCenter", 700)
    state.bandpassWidth = setbox.getNumber("bandpassWidth", 500)
    state.notchEnabled = setbox.getBool("notchEnabled", false)
    state.notchCenter = setbox.getNumber("notchCenter", 0)
    state.notchWidth = setbox.getNumber("notchWidth", 100)

    -- Hardware settings
    state.qsdOffsetK = setbox.getNumber("qsdOffsetK", 12.0)
    state.rfAttenDb = math.floor(setbox.getNumber("rfAttenDb", 0))

    -- Waterfall/display settings
    wfBins = math.floor(setbox.getNumber("wfBins", 512))
    wfRows = math.floor(setbox.getNumber("wfRows", 256))
    state.wfColormap = setbox.getString("wfColormap", "viridis")
    state.wfMinDb = setbox.getNumber("wfMinDb", -120)
    state.wfMaxDb = setbox.getNumber("wfMaxDb", -40)

    -- Hardware connection settings
    hwSettings.host = setbox.getString("hwHost", "127.0.0.1")
    hwSettings.controlPort = math.floor(setbox.getNumber("hwControlPort", 5000))
    hwSettings.streamPort = math.floor(setbox.getNumber("hwStreamPort", 5001))

    print(string.format("[Lua] Config loaded: freq=%.3f MHz, mode=%s, band=%s",
        state.frequency, state.selectedMode, state.selectedBand))
    print(string.format("[Lua] VFO A=%.3f MHz, B=%.3f MHz, active=%s",
        state.vfoA, state.vfoB, state.activeVFO))
    print(string.format("[Lua] DSP: squelch=%.2f, AGC=%s, NR=%s, NB=%s",
        state.squelch, tostring(state.agcEnabled), tostring(state.nrEnabled), tostring(state.nbEnabled)))
    print(string.format("[Lua] Hardware: QSD k=%.1f kHz, atten=%d dB",
        state.qsdOffsetK, state.rfAttenDb))
    print(string.format("[Lua] Display: waterfall %dx%d, colormap=%s, range=[%d,%d] dB",
        wfBins, wfRows, state.wfColormap, state.wfMinDb, state.wfMaxDb))

    -- ==========================================================================
    -- Initialize subsystems
    -- ==========================================================================

    -- Background color
    local bg = setbox.getString("background", "#1a1a2e")
    local r, g, b = hexToRgb(bg)
    setClearColor(r, g, b)

    -- Initialize audio (convert dB to linear gain for C++ audio engine)
    if audio.isInitialized() then
        local linearGain = math.pow(10, state.volumeDb / 20)
        audio.setVolume(linearGain)
        audio.setMuted(state.muteEnabled)
        audio.start()
        print("[Lua] Audio started at " .. audio.getSampleRate() .. " Hz")
    else
        print("[Lua] Warning: Audio not initialized")
    end

    -- Initialize baseband filter with current settings
    rx.setBandpassEnabled(state.bandpassEnabled)
    rx.setBandpassCenter(state.bandpassCenter)
    rx.setBandpassWidth(state.bandpassWidth)
    rx.setNotchEnabled(state.notchEnabled)
    rx.setNotchCenter(state.notchCenter)
    rx.setNotchWidth(state.notchWidth)

    -- Initialize waterfall
    if waterfall.init(wfBins, wfRows) then
        waterfall.setRange(state.wfMinDb, state.wfMaxDb)
        applyColormap(state.wfColormap)
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

    -- ==========================================================================
    -- Initialize new architecture modules
    -- ==========================================================================

    -- Initialize bands module and set initial frequency
    bands.init()
    bands.setCurrent(state.frequency * 1e6)  -- Pass Hz to bands module
    print(string.format("[Lua] Band detection initialized, current band: %s", bands.getCurrent() or "OOB"))

    -- Initialize events module and wire to layout/widgets
    events.init()
    layout.setEventsModule(events)
    ui.setEventsModule(events)
    ui.setLayoutModule(layout)

    -- =======================================================================
    -- Property Accessor Registry
    -- Auto-generated from propertySpecs, plus special-case properties
    -- =======================================================================

    -- Generate accessors from property specs
    -- Each generated accessor: get() returns state[name], set(v) clamps/steps then stores and calls setter
    local function generateAccessors(specs)
        local accessors = {}
        for name, spec in pairs(specs) do
            accessors[name] = {
                get = function() return state[name] end,
                set = function(v)
                    -- Clamp to bounds if specified
                    if spec.min ~= nil then v = math.max(spec.min, v) end
                    if spec.max ~= nil then v = math.min(spec.max, v) end
                    -- Quantize to step if specified
                    if spec.step then v = math.floor(v / spec.step + 0.5) * spec.step end
                    -- Store value
                    state[name] = v
                    -- Call C++ setter if specified (optionally check hardware connection)
                    if spec.setter then
                        if not spec.requiresHw or hw.isConnected() then
                            spec.setter(v)
                        end
                    end
                end,
            }
        end
        return accessors
    end

    -- Auto-generate accessors from specs (populates module-level propertyAccessors)
    propertyAccessors = generateAccessors(propertySpecs)

    -- Special-case properties with custom logic (not auto-generated)
    local bandFreqs = {
        ["160m"] = 1.9, ["80m"] = 3.5, ["40m"] = 7.0,
        ["20m"] = 14.0, ["15m"] = 21.0, ["10m"] = 28.0,
    }

    propertyAccessors.frequency = {
        get = function() return state.frequency end,
        set = function(v)
            state.frequency = clamp(v, 0.1, 30.0)
            if state.activeVFO == "A" then state.vfoA = state.frequency else state.vfoB = state.frequency end
            bands.setCurrent(state.frequency * 1e6)
        end,
    }

    propertyAccessors.selectedMode = {
        get = function() return state.selectedMode end,
        set = function(v)
            state.selectedMode = v
            modeHelper.setMode(v)
        end,
    }

    propertyAccessors.selectedBand = {
        get = function() return state.selectedBand end,
        set = function(v)
            if bandFreqs[v] then
                state.selectedBand = v
                state.frequency = bandFreqs[v]
                if state.activeVFO == "A" then state.vfoA = state.frequency else state.vfoB = state.frequency end
                bands.setCurrent(state.frequency * 1e6)
            end
        end,
    }

    propertyAccessors.wfColormap = {
        get = function() return state.wfColormap end,
        set = function(v)
            state.wfColormap = v
            applyColormap(v)
        end,
    }

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
            if event.key == "Right" or event.key == "Up" then delta = 1
            elseif event.key == "Left" or event.key == "Down" then delta = -1
            else return false end
        end
        if delta == 0 then return false end

        -- Get step from rule (required)
        local step = props and props.step
        if not step then return false end

        -- Apply to frequency (VFO control always operates on frequency)
        local current = state.frequency
        local newVal = current + delta * step
        newVal = math.max(0.1, math.min(30.0, newVal))

        -- Update state
        state.frequency = newVal
        if state.activeVFO == "A" then state.vfoA = newVal else state.vfoB = newVal end
        bands.setCurrent(newVal * 1e6)

        if events.debugDispatch then
            print(string.format("[vfo_control] freq=%.6f step=%s delta=%d", newVal, step, delta))
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
        if newFreq and newFreq >= 0.1 and newFreq <= 30.0 then
            state.frequency = newFreq
            if state.activeVFO == "A" then state.vfoA = state.frequency else state.vfoB = state.frequency end
            bands.setCurrent(state.frequency * 1e6)
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
        state.activeVFO = "A"
        state.frequency = state.vfoA
        bands.setCurrent(state.frequency * 1e6)
        return true
    end)

    events.registerHandler("vfo_b_click", function(event, widget)
        state.activeVFO = "B"
        state.frequency = state.vfoB
        bands.setCurrent(state.frequency * 1e6)
        return true
    end)

    events.registerHandler("vfo_swap_click", function(event, widget)
        state.vfoA, state.vfoB = state.vfoB, state.vfoA
        state.frequency = state.activeVFO == "A" and state.vfoA or state.vfoB
        bands.setCurrent(state.frequency * 1e6)
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
        rxActive = not rxActive
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
    -- Event dispatch via SetBox
    -- =======================================================================

    local mouseX, mouseY = getMousePos()

    -- Return all currently active tags (held keys, held buttons, mode tags)
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

    -- =======================================================================
    -- Keyboard event dispatch
    -- Check ALL keys for KeyDown and KeyUp events (including modifiers)
    -- =======================================================================

    -- Build modifiers array once for all key events (namespaced)
    local modifiers = {}
    if shift then table.insert(modifiers, "input.SHIFT") end
    if ctrl then table.insert(modifiers, "input.CTRL") end
    if alt then table.insert(modifiers, "input.ALT") end

    -- Check ALL keys for dispatch - rules decide what's handled, not hardcoded lists
    for _, scancode in ipairs(keys.getAllScancodes()) do
        local isDown = isKeyDown(scancode)
        local wasDown = keyStates[scancode]

        local keyName = keys.getName(scancode)
        -- Namespace the key tag as input.* (e.g., input.H, input.LSHIFT)
        local inputTag = keyName and ("input." .. keyName) or nil

        if isDown and not wasDown then
            -- Key just pressed - add to active tags and dispatch KeyDown
            if inputTag then activeTags[inputTag] = true end
            local keyEvent = events.createEvent(events.Type.KEY_DOWN, {
                scancode = scancode,
                key = keyName,  -- Keep key name without namespace for event.KeyDown-H format
                char = keys.getChar(scancode),
                isModifier = keys.isModifier(scancode),
                x = mouseX,
                y = mouseY,
                modifiers = getActiveTags(),
                shift = shift,
                ctrl = ctrl,
                alt = alt,
            })
            events.dispatchKey(keyEvent)
        elseif wasDown and not isDown then
            -- Key just released - remove from active tags and dispatch KeyUp
            if inputTag then activeTags[inputTag] = nil end
            local keyEvent = events.createEvent(events.Type.KEY_UP, {
                scancode = scancode,
                key = keyName,  -- Keep key name without namespace for event.KeyUp-H format
                char = keys.getChar(scancode),
                isModifier = keys.isModifier(scancode),
                x = mouseX,
                y = mouseY,
                modifiers = getActiveTags(),
                shift = shift,
                ctrl = ctrl,
                alt = alt,
            })
            events.dispatchKey(keyEvent)
        end

        -- Update state for next frame
        keyStates[scancode] = isDown
    end

    -- Dispatch text input (for frequency entry mode - handled via state.FreqEntryMode tag)
    local textIn = getTextInput()
    if #textIn > 0 and events.hasModeTag("state.FreqEntryMode") then
        local textEvent = events.createEvent(events.Type.TEXT_INPUT, {
            text = textIn,
            x = mouseX,
            y = mouseY,
            modifiers = modifiers,
            shift = shift,
            ctrl = ctrl,
            alt = alt,
        })
        -- Dispatch through SetBox to find handler for FreqEntryMode + TextInput
        events.dispatchKey(textEvent)
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

    -- Send VFO changes to hardware (frequency is in MHz, convert to Hz)
    local currentVfoHz = state.frequency * 1e6
    if currentVfoHz ~= lastVfoFreq then
        rx.setVfo(currentVfoHz)
        lastVfoFreq = currentVfoHz
    end
end

-- =======================================================================
-- UI Helper Functions
-- Reduce boilerplate for common UI patterns
-- =======================================================================

-- Draw a labeled slider that reads/writes from property system
-- Uses propertySpecs for min/max bounds, property system for get/set
local function drawPropertySlider(id, label, prop, w, fmt)
    local spec = propertySpecs[prop]
    local lx, ly = layout.getCursor()
    ui.label(id .. "-label", lx, ly, label .. ":")
    local minVal = spec and spec.min or 0
    local maxVal = spec and spec.max or 1
    local current = getProperty(prop)
    local tags = spec and spec.logScale and {"LogScale"} or nil
    local newVal = ui.slider(id, lx + 55, ly, w - 85, minVal, maxVal, current, tags, prop)
    if newVal ~= current then
        setProperty(prop, newVal)
    end
    local text = string.format(fmt or "%.0f", newVal)
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
        events.registerWidget("top-bar", {x=x, y=y, w=w, h=h}, {"StatusBar", "UIPanel"}, nil)

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
        layout.endRegion()
    end

    -- =========================================
    -- BOTTOM BAR - Status info
    -- =========================================
    do
        local r = regions["bottom-bar"]
        local x, y, w, h = r.x, r.y, r.w, r.h
        layout.setRegion(x, y, w, h, "bottom-bar")

        -- Register bottom bar for event dispatch (ID must match container.lua's widgetOrder)
        events.registerWidget("bottom-bar", {x=x, y=y, w=w, h=h}, {"StatusBar", "UIPanel"}, nil)

        drawRect(x, y, w, h, 0.08, 0.08, 0.1, 1.0)
        drawLine(x, y, x + w, y, 0.2, 0.2, 0.25, 1.0, 1.0)

        local bpInfo = state.bandpassEnabled and string.format("BP: %.0f Hz", state.bandpassWidth) or "BP: Off"
        local info = string.format("Band: %s | Mode: %s | %s | Mouse: %d, %d",
                                   bands.getCurrent() or "OOB", state.selectedMode, bpInfo, mouseX, mouseY)
        drawText(x + 10, y + 6, info, 0.5, 0.5, 0.55, 1.0)
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
        local freqStr = string.format("%.6f", state.frequency)
        local freqW = measureText(freqStr)
        local fx, fy = layout.getCursor()
        local sidebarParent = {width = w, height = h}
        local freqBoxW = getLayoutSize({"widget.FrequencyDisplay"}, "width", sidebarParent, "sidebar-freq-display") or (w - 24)
        local freqBoxH = getLayoutSize({"widget.FrequencyDisplay"}, "height", sidebarParent, "sidebar-freq-display") or 36
        drawRoundedRect(fx, fy, freqBoxW, freqBoxH, 4, 0.1, 0.1, 0.15, 1.0)
        drawText(fx + (freqBoxW - freqW) / 2, fy + (freqBoxH - 16) / 2, freqStr, 0.2, 0.9, 0.4, 1.0)
        -- Register for wheel tuning and click to enter frequency
        events.registerWidget("sidebar-freq-display",
            {x = fx, y = fy, w = freqBoxW, h = freqBoxH},
            {"FrequencyDisplay", "VFOControl"},
            nil)
        layout.newLine(freqBoxH + 4)

        -- Frequency slider (wheel/arrow handled via VFOControl rules in events.lua)
        local sx, sy = layout.getCursor()
        local newFreq = ui.slider("freq-slider", sx, sy, w - 24, 1.0, 30.0, state.frequency, {"VFOControl"}, "frequency")
        if newFreq ~= state.frequency then
            state.frequency = newFreq
            if state.activeVFO == "A" then state.vfoA = state.frequency else state.vfoB = state.frequency end
            bands.setCurrent(state.frequency * 1e6)
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
        local rxLabel = rxActive and "RX ON" or "RX OFF"
        ui.toggle("rx-toggle", rxLabel, toggleX, toggleY, w - 24, 40, rxActive, {"RxToggle"})
        layout.newLine(52)

        local sepX, sepY = layout.getCursor()
        ui.separator("sep-rx-smeter", sepX, sepY, w - 24)
        layout.newLine(12)

        -- S-Meter
        local lx, ly = layout.getCursor()
        ui.label("smeter-title", lx, ly, "S-Meter", {"Title"})
        layout.newLine(24)

        local mx, my = layout.getCursor()
        local meterParent = {width = w, height = h}
        local mw = getLayoutSize({"widget.SMeter"}, "width", meterParent, "s-meter") or (w - 24)
        local mh = getLayoutSize({"widget.SMeter"}, "height", meterParent, "s-meter") or 28

        -- Register S-meter as a widget
        events.registerWidget("s-meter", {x=mx, y=my, w=mw, h=mh}, {"Meter", "SMeter"}, nil)

        drawRoundedRect(mx, my, mw, mh, 4, 0.12, 0.12, 0.15, 1.0)

        -- Get signal level from smeter module (Lua calculates from RMS)
        local sig = smeter.getReading()

        -- Map S-units to bar display (0-9 = bars 0-8, 9+ = bars 9+)
        -- Bar 0-8 = S1-S9, bar 9 = S9+10, etc
        local barCount = 12  -- S1-S9 + S9+10, +20, +30
        local barPad = 4
        local barGap = 2
        local barW = (mw - barPad * 2 - barGap * (barCount - 1)) / barCount
        local barH = mh - barPad * 2
        for i = 0, barCount - 1 do
            local barX = mx + barPad + i * (barW + barGap)
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
                drawRect(barX, my + barPad, barW, barH, r, g, b, 1.0)
            else
                drawRect(barX, my + barPad, barW, barH, 0.2, 0.2, 0.25, 1.0)
            end
        end
        layout.newLine(mh + 12)

        -- Display S-meter reading (text comes from smeter module)
        local tx, ty = layout.getCursor()
        local textParent = {width = mw, height = h}  -- inherit meter width for context
        local textW = getLayoutSize({"widget.SMeterText"}, "width", textParent, "smeter-text") or mw
        local textH = getLayoutSize({"widget.SMeterText"}, "height", textParent, "smeter-text") or 20
        -- Register S-meter text display as widget
        events.registerWidget("smeter-text", {x=tx, y=ty, w=textW, h=textH}, {"Label", "SMeterText"}, nil)
        -- Center text within the widget bounds
        local sTextW = measureText(sig.sText)
        local dBmTextW = measureText(sig.dBmText)
        local totalTextW = sTextW + 10 + dBmTextW
        local textStartX = tx + (textW - totalTextW) / 2
        drawText(textStartX, ty + 2, sig.sText, 0.9, 0.9, 0.95, 1.0)
        drawText(textStartX + sTextW + 10, ty + 2, sig.dBmText, 0.5, 0.5, 0.55, 1.0)
        layout.newLine(textH + 4)

        sepX, sepY = layout.getCursor()
        ui.separator("sep-smeter-band", sepX, sepY, w - 24)
        layout.newLine(12)

        -- Band Buttons
        lx, ly = layout.getCursor()
        ui.label("band-title", lx, ly, "Band", {"Title"})
        layout.newLine(24)

        local bandNames = {"160m", "80m", "40m", "20m", "15m", "10m"}

        for i, bandName in ipairs(bandNames) do
            if i % 2 == 1 then
                layout.beginHorizontal(4)
            end

            local bx, by = layout.reserveSpace(80, 26)
            -- Tag format: "Band160m", "Band80m" etc. for SetBox routing
            local bandTag = "Band" .. bandName
            local bandBtnTags = bands.getCurrent() == bandName and {"Active", bandTag} or {bandTag}
            ui.button("band-" .. bandName, bandName, bx, by, 80, 26, bandBtnTags)

            if i % 2 == 0 or i == #bandNames then
                layout.endHorizontal()
            end
        end

        layout.space(8)
        sepX, sepY = layout.getCursor()
        ui.separator("sep-band-display", sepX, sepY, w - 24)
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
        local r = regions["center"]
        local x, y, w, h = r.x, r.y, r.w, r.h
        layout.setRegion(x, y, w, h, "center")
        layout.pad(8)
        local cx, cy, cw, ch = layout.getRect()

        -- Panel background
        ui.panel("center-panel", cx, cy, cw, ch)
        layout.pad(8)

        local px, py, pw, ph = layout.getRect()

        -- Title bar with frequency info (or entry field)
        local freqStr
        local freqColor = {0.2, 0.9, 0.4}
        local inFreqEntry = events.hasModeTag("state.FreqEntryMode")
        if inFreqEntry then
            -- Show entry text with blinking cursor
            local cursor = freqEntryBlink < 0.5 and "_" or ""
            freqStr = freqEntryText .. cursor .. " MHz [Enter=OK, Esc=Cancel]"
            freqColor = {1.0, 0.9, 0.2}  -- Yellow when editing
        else
            freqStr = string.format("%.6f MHz", state.frequency)
        end
        local freqW = measureText(freqStr)
        drawText(px, py, freqStr, freqColor[1], freqColor[2], freqColor[3], 1.0)

        -- Register frequency display widget for event dispatch
        events.registerWidget("frequency-display",
            {x = px, y = py - 4, w = freqW + 20, h = 24},
            {"FrequencyDisplay", "VFOControl"},
            nil)

        -- Colormap selector (right side of title, wraps if needed)
        -- Uses colormaps from config/colormaps.lua when available
        local cmapNames = {"viridis", "plasma", "inferno", "green", "blue"}
        local btnW, btnH, btnGap = 52, 20, 4
        local totalBtnW = #cmapNames * (btnW + btnGap) - btnGap
        local availableW = pw - freqW - 40  -- Space after frequency text
        local cmapX = px + pw - math.min(totalBtnW, availableW)
        local cmapY = py - 2
        local rowX = cmapX
        for i, cmap in ipairs(cmapNames) do
            -- Wrap to next row if button would exceed panel width
            if rowX + btnW > px + pw then
                rowX = cmapX
                cmapY = cmapY + btnH + 2
            end
            -- Tag format: "CmapViridis", "CmapPlasma" etc. (capitalize first letter)
            local cmapTag = "Cmap" .. cmap:sub(1,1):upper() .. cmap:sub(2)
            local cmapBtnTags = state.wfColormap == cmap and {"Active", cmapTag} or {cmapTag}
            ui.button("cmap-" .. cmap, cmap, rowX, cmapY, btnW, btnH, cmapBtnTags)
            rowX = rowX + btnW + btnGap
        end

        -- Display area
        local vizY = py + 24
        local vizH = ph - 30

        -- Spectrum height can be overridden (default 35% of viz area from SetBox rules)
        local specParent = {width = pw, height = vizH}
        local defaultSpecH = math.floor(vizH * 0.35)
        local specH = getLayoutSize({"widget.Spectrum"}, "height", specParent, "spectrum-display") or defaultSpecH
        specH = math.max(50, math.min(vizH - 50, specH))  -- Clamp to reasonable bounds
        dispatch.renderSpectrum(spectrumData, px, vizY, pw, specH)

        -- Register spectrum widget for event dispatch
        events.registerWidget("spectrum-display",
            {x = px, y = vizY, w = pw, h = specH},
            {"Spectrum", "VFOControl"},
            nil)

        -- Separator line
        drawLine(px, vizY + specH + 2, px + pw, vizY + specH + 2, 0.3, 0.3, 0.4, 1.0, 1.0)

        -- Waterfall takes remaining space
        local wfY = vizY + specH + 4
        local wfH = vizH - specH - 8
        dispatch.renderWaterfall(px, wfY, pw, wfH)

        -- Register waterfall widget for event dispatch
        events.registerWidget("waterfall-display",
            {x = px, y = wfY, w = pw, h = wfH},
            {"Waterfall", "VFOControl"},
            nil)

        -- Center frequency marker
        local centerX = px + pw / 2
        drawLine(centerX, vizY, centerX, vizY + vizH, 0.9, 0.3, 0.3, 0.5, 1.0)

        -- Frequency scale labels
        local spanKHz = 96  -- Simulated span
        local lowFreqLabel = string.format("-%.0fkHz", spanKHz/2)
        local highFreqLabel = string.format("+%.0fkHz", spanKHz/2)
        local labelY = vizY + vizH - 16
        local labelH = 16

        -- Register frequency labels as widgets
        local lowLabelW = measureText(lowFreqLabel) + 10
        events.registerWidget("freq-label-low",
            {x = px + 5, y = labelY, w = lowLabelW, h = labelH},
            {"Label", "FreqLabel"}, nil)
        drawText(px + 5, labelY, lowFreqLabel, 0.5, 0.5, 0.6, 1.0)

        local highLabelW = measureText(highFreqLabel) + 10
        events.registerWidget("freq-label-high",
            {x = px + pw - highLabelW - 5, y = labelY, w = highLabelW, h = labelH},
            {"Label", "FreqLabel"}, nil)
        drawText(px + pw - highLabelW, labelY, highFreqLabel, 0.5, 0.5, 0.6, 1.0)

        layout.endRegion()
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
