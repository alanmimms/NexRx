--[[
  NexRx Application - Main Lua Entry Point
]]

local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path

local ui = require("ui.Widgets")
local uiState = require("ui.State")
local layout = require("ui.Layout")
local container = require("ui.Container")
_G.bands = require("Bands")
local bands = _G.bands
local Hardware = require("Hardware")
local Model = require("Model")
local AppController = require("AppController")
local events = require("Events")
local animate = require("Animate")
local keys = require("Keycodes")
local modeHelper = require("Modes")
local smeter = require("SMeter")
_G.calibration = require("Calibration")
local Edit = require("Edit")
local layoutOverrides = require("LayoutOverrides")

-- Modular Widget Classes
local Preselector = require("ui.Preselector")
local ISG = require("ui.ISG")
local AGC = require("ui.AGC")
local AudioUtils = require("ui.AudioUtils")
local CWUtils = require("ui.CWUtils")
local Label = require("ui.Label")
local Button = require("ui.Button")
local Checkbox = require("ui.Checkbox")
local Slider = require("ui.Slider")
local Panel = require("ui.Panel")
local SMeter = require("ui.SMeter")
local ActiveTags = require("ui.ActiveTags")
local GraticuleLegend = require("ui.GraticuleLegend")
local FrequencyDisplay = require("ui.FrequencyDisplay")

local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0
local lastMouseX, lastMouseY = 0, 0
local BUTTON_NAMES = {"LEFT", "MIDDLE", "RIGHT"}
_G.isgFreqEntryText = ""
local keyStates = {}
local activeTags = {}

-- Sidebar modular widgets
local widgets = {
    presel = Preselector.new({ preselector = Model.preselector }),
    isg = ISG.new({ ISG = Model.ISG }),
    agc = AGC.new({ AGC = Model.rx.AGC }),
    cw = CWUtils.new({ rx = Model.rx }),
    audio = AudioUtils.new({ rx = Model.rx })
}

-- Persistent widget instances for main UI
local uiInstances = {}

local function setProperty(name, v)
    local prevTags = setbox.getActiveTags()
    setbox.addTag("prop." .. name)
    setbox.setActiveTags(prevTags)
    Model.set(name, v)
end

local function getAllActiveTags()
    local allTags = {}
    
    if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then allTags["input.SHIFT"] = true end
    if activeTags["input.LCTRL"] or activeTags["input.RCTRL"] then allTags["input.CTRL"] = true end
    if activeTags["input.LALT"] or activeTags["input.RALT"] then allTags["input.ALT"] = true end
    
    -- Copy all current input tags
    for k, v in pairs(activeTags) do if v then allTags[k] = true end end

    if events and events.getModeTags then for _, t in ipairs(events.getModeTags()) do allTags[t] = true end end
    
    local mx, my = getMousePos()
    local hovered = events.getWidgetAt(mx, my)
    if hovered and hovered.id then
        -- Add namespaced hover tag
        allTags["state.Hovered:" .. hovered.id] = true
        
        -- Do NOT propagate all tags of the hovered widget to global state.
        -- This was causing global properties (like 'height') to be overridden 
        -- by the hovered widget's rules for ALL widgets.
        -- Instead, only propagate specifically allowed tags if needed.
        if hovered.tags then
            for _, tag in ipairs(hovered.tags) do
                -- Allow certain tags to be global even when hovered
                -- (Currently none strictly required to be global from hover)
            end
        end
    end
    
    if activeTags["input.MouseLEFT"] and hovered and hovered.id then
        -- Add namespaced pressed tag
        allTags["state.Pressed:" .. hovered.id] = true
    end
    
    local list = {}
    for tag, _ in pairs(allTags) do table.insert(list, tag) end
    table.sort(list)
    return list
end

-- Load configuration files
local configFiles = {
    "config/default.lua",
    "config/settings.lua",
    "config/bands.lua", 
    "config/colormaps.lua", 
    "config/events.lua", 
    "config/layout.lua", 
    "config/modes.lua", 
    "config/constraints.lua"
}
for _, file in ipairs(configFiles) do setbox.loadFile(file) end

local hwConnected = false
local freqEntryText = ""
local freqEntryCursor = 0
local freqEntryBlink = 0
local wfBins = 512
local wfRows = 256
local spectrumData = {}
_G.rxStats = { rms=0, gain0=0, phase0=0, gain1=0, phase1=0, w0_mag=0, w1_mag=0 }
_G.isgFreqEntryText = ""
_G.isgFreqEntryCursor = 0

function init()
    AppController.init()
    _G.calibration.init()
    
    wfBins = 1024 -- Match DspEngine::FFT_SIZE
    wfRows = 256 
    for i = 1, wfBins do spectrumData[i] = -100 end
    
    setClearColor(0.1, 0.1, 0.15)
    if audio.isInitialized() then audio.start() end
    if waterfall.init(wfBins, wfRows) then
        waterfall.setRange(Model.waterfall.minDB:get(), Model.waterfall.maxDB:get())
        if _G.colormaps and _G.colormaps[Model.waterfall.colormap:get()] then 
            waterfall.setColormapData(_G.colormaps[Model.waterfall.colormap:get()]) 
        end
        Hardware.enableWaterfall()
    end
    bands.init()
    events.init()
    layout.setEventsModule(events)
    ui.setEventsModule(events)
    ui.setLayoutModule(layout)
    
    if hw.connect("127.0.0.1", 5000, 5001) then
        Hardware.enableHardware()
    end

    -- Create UI Instances with Dependency Injection
    uiInstances.leftPanel = Panel.new()
    uiInstances.rightPanel = Panel.new()
    uiInstances.centerPanel = Panel.new()
    
    uiInstances.freqDisplay = FrequencyDisplay.new({
        valueObs = require("Reactive").computed(function()
            local active = Model.rx.VFO.active:get()
            return (active == "A") and Model.rx.VFO.A:get() or Model.rx.VFO.B:get()
        end)
    })
    
    uiInstances.freqSlider = Slider.new({ 
        valueObs = (Model.rx.VFO.active:peek() == "A") and Model.rx.VFO.A or Model.rx.VFO.B 
    })
    
    uiInstances.volumeSlider = Slider.new({ valueObs = Model.rx.volume.DB, propertyName = "rx.volume.DB" })
    uiInstances.rfGainSlider = Slider.new({ valueObs = Model.rx.RF.gainDB, propertyName = "rx.RF.gainDB" })
    uiInstances.smeter = SMeter.new()
    uiInstances.calBtn = Button.new({ getText = function() return "CALIBRATE" end })
    uiInstances.activeTagsViewer = ActiveTags.new()
    uiInstances.graticuleLegend = GraticuleLegend.new()
    uiInstances.rxToggle = Button.new({ 
        getText = function() return Model.rx.active:get() and "RX ON" or "RX OFF" end,
        onClick = function() Model.set("rx.active", not Model.rx.active:get()) end 
    })
    
    uiInstances.modeLabels = Label.new()
    uiInstances.bandLabels = Label.new()
    uiInstances.volLabel = Label.new()
    uiInstances.rfGainLabel = Label.new()
    uiInstances.smeterLabel = Label.new()

    uiInstances.round1k = Button.new({
        getText = function() return "<0k>" end,
        onClick = function()
            local active = Model.rx.VFO.active:peek()
            Model.roundFrequency("rx.VFO." .. active, 1000)
        end
    })
    uiInstances.round100 = Button.new({
        getText = function() return "<00>" end,
        onClick = function()
            local active = Model.rx.VFO.active:peek()
            Model.roundFrequency("rx.VFO." .. active, 100)
        end
    })

    uiInstances.modeButtons = {}
    for _, m in ipairs(modeHelper.names) do
        uiInstances.modeButtons[m] = Button.new({ 
            getText = function() return m == "BYPASS" and "RAW" or m end,
            onClick = function() Model.set("rx.selectedMode", m) end 
        })
    end

    uiInstances.bandButtons = {}
    local bandList = {"160m","80m","40m","20m","15m","10m"}
    for _, b in ipairs(bandList) do
        uiInstances.bandButtons[b] = Button.new({ 
            getText = function() return b end,
            onClick = function() Model.set("rx.selectedBand", b) end 
        })
    end
end

local function clamp(v, min, max)
    return math.max(min, math.min(max, v))
end

function init()
    AppController.init()
    _G.calibration.init()
    
    wfBins = 1024 -- Match DspEngine::FFT_SIZE
    wfRows = 256 
    for i = 1, wfBins do spectrumData[i] = -100 end
    
    setClearColor(0.1, 0.1, 0.15)
    if audio.isInitialized() then audio.start() end
    if waterfall.init(wfBins, wfRows) then
        waterfall.setRange(Model.waterfall.minDB:get(), Model.waterfall.maxDB:get())
        if _G.colormaps and _G.colormaps[Model.waterfall.colormap:get()] then 
            waterfall.setColormapData(_G.colormaps[Model.waterfall.colormap:get()]) 
        end
        Hardware.enableWaterfall()
    end
    bands.init()
    events.init()
    layout.setEventsModule(events)
    ui.setEventsModule(events)
    ui.setLayoutModule(layout)
    
    if hw.connect("127.0.0.1", 5000, 5001) then
        Hardware.enableHardware()
    end

    -- Create UI Instances with Dependency Injection
    uiInstances.leftPanel = Panel.new()
    uiInstances.rightPanel = Panel.new()
    uiInstances.centerPanel = Panel.new()
    
    uiInstances.freqDisplay = FrequencyDisplay.new({
        valueObs = require("Reactive").computed(function()
            local active = Model.rx.VFO.active:get()
            return (active == "A") and Model.rx.VFO.A:get() or Model.rx.VFO.B:get()
        end)
    })
    
    uiInstances.freqSlider = Slider.new({ 
        valueObs = (Model.rx.VFO.active:peek() == "A") and Model.rx.VFO.A or Model.rx.VFO.B 
    })
    
    uiInstances.volumeSlider = Slider.new({ valueObs = Model.rx.volume.DB, propertyName = "rx.volume.DB" })
    uiInstances.rfGainSlider = Slider.new({ valueObs = Model.rx.RF.gainDB, propertyName = "rx.RF.gainDB" })
    uiInstances.smeter = SMeter.new()
    uiInstances.calBtn = Button.new({ getText = function() return "CALIBRATE" end })
    uiInstances.activeTagsViewer = ActiveTags.new()
    uiInstances.graticuleLegend = GraticuleLegend.new()
    uiInstances.rxToggle = Button.new({ 
        getText = function() return Model.rx.active:get() and "RX ON" or "RX OFF" end,
        onClick = function() Model.set("rx.active", not Model.rx.active:get()) end 
    })
    
    uiInstances.modeLabels = Label.new()
    uiInstances.bandLabels = Label.new()
    uiInstances.volLabel = Label.new()
    uiInstances.rfGainLabel = Label.new()
    uiInstances.smeterLabel = Label.new()

    uiInstances.round1k = Button.new({
        getText = function() return "<0k>" end,
        onClick = function()
            local active = Model.rx.VFO.active:peek()
            Model.roundFrequency("rx.VFO." .. active, 1000)
        end
    })
    uiInstances.round100 = Button.new({
        getText = function() return "<00>" end,
        onClick = function()
            local active = Model.rx.VFO.active:peek()
            Model.roundFrequency("rx.VFO." .. active, 100)
        end
    })

    uiInstances.modeButtons = {}
    for _, m in ipairs(modeHelper.names) do
        uiInstances.modeButtons[m] = Button.new({ 
            getText = function() return m == "BYPASS" and "RAW" or m end,
            onClick = function() Model.set("rx.selectedMode", m) end 
        })
    end

    uiInstances.bandButtons = {}
    local bandList = {"160m","80m","40m","20m","15m","10m"}
    for _, b in ipairs(bandList) do
        uiInstances.bandButtons[b] = Button.new({ 
            getText = function() return b end,
            onClick = function() Model.set("rx.selectedBand", b) end 
        })
    end

    events.registerHandler("vfo_control", function(e, w, p)
        local delta = e.delta or (e.key == "RIGHT" and 1 or (e.key == "LEFT" and -1 or 0))
        local active = Model.rx.VFO.active:peek()
        local isISG = p and p.property == "isgFrequency"
        local prop = isISG and "isgFrequency" or ((active == "A") and "rx.VFO.A" or "rx.VFO.B")
        
        if delta ~= 0 and p and p.step then 
            local current = isISG and Model.ISG.frequencyHz:peek() or ((active == "A") and Model.rx.VFO.A:peek() or Model.rx.VFO.B:peek())
            local newVal = clamp(current + delta * p.step, 0.1e6, 30.0e6)
            setProperty(prop, newVal)
            if not isISG then bands.setCurrent(newVal) end
            return true 
        end
        return false
    end)

    events.registerHandler("toggle_property", function(e, w, p)
        local prop = p.property or (w and w.data and w.data.property)
        if prop then 
            Model.set(prop, not setbox.getBool(prop))
            return true 
        end
        return false
    end)
    events.registerHandler("slider_activate", function(e, w, p)
        if w and w.id then uiState.setActive(w.id); return true end
        return false
    end)
    events.registerHandler("slider_adjust", function(e, w, p)
        local prop = p.property or (w and w.data and w.data.property)
        if not prop then return false end
        local val = setbox.getNumber(prop)
        local step = p.step or (w and w.data and (w.data.max - w.data.min) * 0.01) or 0.01
        if e.delta then 
            local newVal = val + e.delta * step
            setProperty(prop, newVal)
            return true 
        end
        return false
    end)
    events.registerHandler("set_value", function(e, w, p)
        if p and p.property and p.value ~= nil then setProperty(p.property, p.value) return true end
        return false
    end)

    -- Frequency Entry Handlers
    events.registerHandler("freq_entry_start", function(event, widget)
        -- If already in frequency entry mode, gobble the event but don't restart
        if events.hasModeTag("state.FreqEntryMode") then
            return true
        end

        if widget and widget.tags and table.concat(widget.tags, ","):find("IsgControl") then
            _G.isgFreqEntryText = ""
            _G.isgFreqEntryCursor = 0
            events.addModeTag("state.IsgFreqEntryMode")
            events.addModeTag("state.FreqEntryMode")
            events.addModeTag("state.ISGEditing")
        else
            freqEntryText = ""
            freqEntryCursor = 0
            events.addModeTag("state.FreqEntryMode")
            events.addModeTag("state.VFOEditing")
        end
        return true
    end)

    events.registerHandler("freq_entry_cancel", function(event, widget)
        events.removeModeTag("state.FreqEntryMode")
        events.removeModeTag("state.IsgFreqEntryMode")
        events.removeModeTag("state.VFOEditing")
        events.removeModeTag("state.ISGEditing")
        freqEntryText = ""; _G.isgFreqEntryText = ""
        freqEntryCursor = 0; _G.isgFreqEntryCursor = 0
        return true
    end)

    events.registerHandler("freq_entry_backspace", function(event, widget)
        if events.hasModeTag("state.IsgFreqEntryMode") then
            if _G.isgFreqEntryCursor > 0 then
                _G.isgFreqEntryText = _G.isgFreqEntryText:sub(1, _G.isgFreqEntryCursor - 1) .. _G.isgFreqEntryText:sub(_G.isgFreqEntryCursor + 1)
                _G.isgFreqEntryCursor = _G.isgFreqEntryCursor - 1
            end
        elseif events.hasModeTag("state.FreqEntryMode") then
            if freqEntryCursor > 0 then
                freqEntryText = freqEntryText:sub(1, freqEntryCursor - 1) .. freqEntryText:sub(freqEntryCursor + 1)
                freqEntryCursor = freqEntryCursor - 1
            end
        end
        return true
    end)

    events.registerHandler("freq_entry_text", function(event, widget)
        local text = event.text or ""
        for i = 1, #text do
            local ch = text:sub(i, i)
            if ch == "," then ch = "." end
            if ch:match("[0-9]") or (ch == ".") then
                if events.hasModeTag("state.IsgFreqEntryMode") then
                    if ch ~= "." or not _G.isgFreqEntryText:find("%.") then
                        _G.isgFreqEntryText = _G.isgFreqEntryText:sub(1, _G.isgFreqEntryCursor) .. ch .. _G.isgFreqEntryText:sub(_G.isgFreqEntryCursor + 1)
                        _G.isgFreqEntryCursor = _G.isgFreqEntryCursor + 1
                    end
                elseif events.hasModeTag("state.FreqEntryMode") then
                    if ch ~= "." or not freqEntryText:find("%.") then
                        freqEntryText = freqEntryText:sub(1, freqEntryCursor) .. ch .. freqEntryText:sub(freqEntryCursor + 1)
                        freqEntryCursor = freqEntryCursor + 1
                    end
                end
            end
        end
        return true
    end)

    events.registerHandler("freq_entry_confirm", function(event, widget)
        if events.hasModeTag("state.IsgFreqEntryMode") then
            local newFreq = tonumber(_G.isgFreqEntryText)
            if newFreq then 
                local val = clamp(newFreq * 1e6, 0.1e6, 30.0e6)
                setProperty("isgFrequency", val) 
            end
            events.removeModeTag("state.IsgFreqEntryMode")
            events.removeModeTag("state.FreqEntryMode")
            events.removeModeTag("state.ISGEditing")
            _G.isgFreqEntryText = ""; _G.isgFreqEntryCursor = 0
        elseif events.hasModeTag("state.FreqEntryMode") then
            local newFreq = tonumber(freqEntryText)
            if newFreq then
                local val = clamp(newFreq * 1e6, 0.1e6, 30.0e6)
                local active = Model.rx.VFO.active:peek()
                Model.set("rx.VFO." .. active, val)
                bands.setCurrent(val)
            end
            events.removeModeTag("state.FreqEntryMode")
            events.removeModeTag("state.VFOEditing")
            freqEntryText = ""; freqEntryCursor = 0
        end
        return true
    end)

    events.registerHandler("freq_entry_move", function(event, widget)
        local delta = (event.key == "RIGHT") and 1 or -1
        if events.hasModeTag("state.IsgFreqEntryMode") then
            _G.isgFreqEntryCursor = math.max(0, math.min(#_G.isgFreqEntryText, _G.isgFreqEntryCursor + delta))
        elseif events.hasModeTag("state.FreqEntryMode") then
            freqEntryCursor = math.max(0, math.min(#freqEntryText, freqEntryCursor + delta))
        end
        return true
    end)

    Edit.init(events)
    layoutOverrides.load()
end

local currentFrameTags = {}

function update(dt)
    frameCount = frameCount + 1
    fpsAccum = fpsAccum + dt; fpsFrames = fpsFrames + 1
    if fpsAccum >= 1.0 then 
        fps = fpsFrames / fpsAccum; fpsAccum = 0; fpsFrames = 0 
    end
    animate.update(dt)
    freqEntryBlink = (freqEntryBlink + dt) % 1.0
    
    local scancodes = keys.getAllScancodes()
    for _, sc in ipairs(scancodes) do
        local isDown = isKeyDown(sc)
        local n = keys.getName(sc)
        if n then
            local prevDown = activeTags["input."..n]
            activeTags["input."..n] = isDown or nil
            
            if isDown and not prevDown then
                -- Key Pressed
                events.dispatch(events.createEvent(events.Type.KEY_DOWN, {key=n, scancode=sc, modifiers=currentFrameTags}))
                
                -- Also handle text input for printable keys
                if keys.isPrintable(sc) then
                    local char = isShiftDown() and keys.getShiftedChar(sc) or keys.getChar(sc)
                    if char then
                        events.dispatch(events.createEvent(events.Type.TEXT_INPUT, {text=char, modifiers=currentFrameTags}))
                    end
                end
            elseif not isDown and prevDown then
                -- Key Released
                events.dispatch(events.createEvent(events.Type.KEY_UP, {key=n, scancode=sc, modifiers=currentFrameTags}))
            end
        end
    end
    
    activeTags["input.SHIFT"] = isShiftDown() or nil
    activeTags["input.CTRL"] = isCtrlDown() or nil
    activeTags["input.ALT"] = isAltDown() or nil
    activeTags["input.MouseLEFT"] = isMouseDown(0) or nil
    activeTags["input.MouseMIDDLE"] = isMouseDown(1) or nil
    activeTags["input.MouseRIGHT"] = isMouseDown(2) or nil

    local mx, my = getMousePos()
    currentFrameTags = getAllActiveTags()
    setbox.setActiveTags(currentFrameTags)
    uiState.beginFrame()
    
    local wheel = getMouseWheel()
    if wheel ~= 0 then events.dispatch(events.createEvent(events.Type.MOUSE_WHEEL, {x=mx,y=my,delta=wheel,modifiers=currentFrameTags})) end
    for b = 0, 2 do
        if isMouseClicked(b) then events.dispatch(events.createEvent(events.Type.MOUSE_DOWN, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=currentFrameTags}))
        elseif isMouseReleased(b) then events.dispatch(events.createEvent(events.Type.MOUSE_UP, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=currentFrameTags})) end
    end
    if mx ~= lastMouseX or my ~= lastMouseY then
        events.dispatch(events.createEvent(events.Type.MOUSE_MOVE, {x=mx,y=my,dx=mx-lastMouseX,dy=my-lastMouseY,modifiers=currentFrameTags}))
        lastMouseX, lastMouseY = mx, my
    end
    
    -- Hardware Synchronization Round
    AppController.sync()

    if Hardware.isHardwareEnabled() then
        if frameCount % 10 == 0 then
            AppController.pollState()
        end
        
        local hwSpec = Hardware.getSpectrum()
        if hwSpec and #hwSpec > 0 then spectrumData = hwSpec; Hardware.updateWaterfall(spectrumData) end
        if frameCount % 10 == 0 then
            local s = rx.getStats()
            if s then
                _G.rxStats.rms = s.rms
                _G.rxStats.gain0 = s.gain0
                _G.rxStats.phase0 = s.phase0
                _G.rxStats.gain1 = s.gain1
                _G.rxStats.phase1 = s.phase1
                _G.rxStats.w0_mag = s.w0_mag
                _G.rxStats.w1_mag = s.w1_mag
            end
        end
    else
        Hardware.updateWaterfall(spectrumData)
    end
end

function draw()
    local winW, winH = getWindowSize()
    local mx, my = getMousePos()
    local tags = currentFrameTags
    events.clearWidgets()
    ui.beginFrame()
    setbox.setActiveTags(tags)
    
    layout.begin(0, 0, winW, winH)
    local regions = container.solve(winW, winH)
    if not regions then return end

    -- Top Bar
    local rT = regions["top-bar"]
    if rT then
        layout.setRegion(rT.x, rT.y, rT.w, rT.h, "top-bar")
        events.registerWidget("top-bar", rT, {"widget.StatusBar"})
        drawRect(rT.x, rT.y, rT.w, rT.h, 0.08, 0.08, 0.12, 1.0)
        local fC = Hardware.isHardwareEnabled() and hw.getFramesReceived and hw.getFramesReceived() or 0
        drawText(rT.x + 10, rT.y + 8, string.format("NexRx | %.0f FPS | Frames: %d", fps, fC), 0.6, 0.7, 0.9, 1.0)
        layout.endRegion()
    end

    -- Left Sidebar
    local rL = regions["left-sidebar"]
    if rL then
        local lwc = setbox.newContext({"widget.Sidebar", "widget.LeftSidebar", "id.left-sidebar"})
        layout.setRegion(rL.x, rL.y, rL.w, rL.h, "left-sidebar")
        uiInstances.leftPanel:draw("left-sidebar", rL.x, rL.y, rL.w, rL.h, lwc)
        layout.pad(12)
        local cx, cy = layout.getCursor()
        uiInstances.freqDisplay:draw("freq-disp", cx, cy, rL.w - 24, 36, freqEntryText, freqEntryCursor, {"VFOControl"}, lwc)
        layout.newLine(40)
        
        -- Rounding buttons
        layout.beginHorizontal(0)
        local rbw = (rL.w - 24 - 4) / 2
        uiInstances.round1k:draw("round-1k", cx, layout.getCursorY(), rbw, 24, {}, lwc)
        layout.space(4)
        uiInstances.round100:draw("round-100", cx + rbw + 4, layout.getCursorY(), rbw, 24, {}, lwc)
        layout.endHorizontal(); layout.newLine(32)

        cx, cy = layout.getCursor()
        local freq = (Model.rx.VFO.active:get() == "A") and Model.rx.VFO.A:get() or Model.rx.VFO.B:get()
        uiInstances.freqSlider:draw("freq-slider", cx, cy, rL.w - 24, 0.1e6, 30.0e6, freq, lwc)
        layout.newLine(32)

        cx, cy = layout.getCursor(); 
        uiInstances.modeLabels:draw("mode-label", cx, cy, lwc); 
        layout.newLine(24)
        
        layout.beginHorizontal(0)
        for _, m in ipairs(modeHelper.names) do
            local bx, by = layout.reserveSpace(40, 26)
            local active = Model.rx.selectedMode:get() == m and {"state.Active"} or {}
            local btn = uiInstances.modeButtons[m]
            btn:draw("mode-"..m:lower(), bx, by, 40, 26, active, lwc)
            layout.space(4)
        end
        layout.endHorizontal(); layout.newLine(32)

        local bcx, bcy = layout.getCursor(); 
        uiInstances.bandLabels:draw("band-label", bcx, bcy, lwc); 
        layout.newLine(24)

        layout.beginHorizontal(0)
        local bandList = {"160m","80m","40m","20m","15m","10m"}
        for i, b in ipairs(bandList) do
            local bx, by = layout.reserveSpace(40, 26)
            local active = Model.rx.selectedBand:get() == b and {"state.Active"} or {}
            uiInstances.bandButtons[b]:draw("band-"..b, bx, by, 40, 26, active, lwc)
            if i < #bandList then layout.space(4) end
        end
        layout.endHorizontal(); layout.newLine(32)

        cx, cy = layout.getCursor(); 
        uiInstances.volLabel:draw("vol-label", cx, cy, lwc); 
        layout.newLine(24)
        cx, cy = layout.getCursor()
        uiInstances.volumeSlider:draw("volume-slider", cx, cy, rL.w - 24, -60, 0, Model.rx.volume.DB:get(), lwc)
        layout.newLine(32)

        cx, cy = layout.getCursor(); 
        uiInstances.rfGainLabel:draw("rfg-label", cx, cy, lwc); 
        layout.newLine(24)
        cx, cy = layout.getCursor()
        uiInstances.rfGainSlider:draw("rfg-slider", cx, cy, rL.w - 24, -20, 60, Model.rx.RF.gainDB:get(), lwc)
        layout.newLine(24)
        layout.endRegion()
    end

    -- Center Area
    local rC = regions["center-area"]
    if rC then
        uiInstances.centerPanel:draw("center-panel", rC.x, rC.y, rC.w, rC.h)
        local sub = container.solveSublayout(rC, {
            { id = "spec", tags = {"widget.Spectrum"},  anchor = "top", group = "center-col" },
            { id = "wf",   tags = {"widget.Waterfall"}, anchor = "top", group = "center-col" },
        })
        if sub["spec"] then
            local rs = sub["spec"]
            layout.setRegion(rs.x, rs.y, rs.w, rs.h, "spec")
            Hardware.renderSpectrum(spectrumData, rs.x, rs.y, rs.w, rs.h)
            uiInstances.graticuleLegend:draw("spec-legend", rs.x + 10, rs.y + 10, 100, 45, "9.6 kHz/div", "20 dB/div")
            events.registerWidget("spec", rs, {"widget.Spectrum", "widget.VFOControl"})
            layout.endRegion()
        end
        if sub["wf"] then
            local rw = sub["wf"]
            layout.setRegion(rw.x, rw.y, rw.w, rw.h, "wf")
            Hardware.renderWaterfall(rw.x, rw.y, rw.w, rw.h)
            events.registerWidget("wf", rw, {"widget.Waterfall", "widget.VFOControl"})
            layout.endRegion()
        end
    end

    -- Right Sidebar
    local rR = regions["right-sidebar"]
    if rR then
        local lwc = setbox.newContext({"widget.Sidebar", "widget.RightSidebar", "id.right-sidebar"})
        layout.setRegion(rR.x, rR.y, rR.w, rR.h, "right-sidebar")
        uiInstances.rightPanel:draw("right-sidebar", rR.x, rR.y, rR.w, rR.h, lwc)
        layout.pad(12)
        
        local cx, cy = layout.getCursor()
        uiInstances.rxToggle:draw("rx-toggle", cx, cy, rR.w - 24, 40, Model.rx.active:get() and {"state.Active"} or {}, lwc)
        layout.newLine(52)
        
        cx, cy = layout.getCursor(); 
        uiInstances.smeterLabel:draw("sm-label", cx, cy, lwc); 
        layout.newLine(24)
        
        cx, cy = layout.getCursor(); 
        uiInstances.smeter:draw("s-meter", cx, cy, rR.w - 24, 28, smeter.getReading(), lwc); 
        layout.newLine(48)

        -- Calibration Trigger
        local btnW = rR.w - 40
        cx, cy = layout.getCursor()
        if uiInstances.calBtn:draw("cal-btn", cx + 8, cy, btnW, 30, {}, lwc) then
            hw.calibrate()
        end
        layout.newLine(40)
        
        cx, cy = layout.getCursor()
        local remH = (rR.y + rR.h) - cy - 12
        local currentR = { x = rR.x + 12, y = cy, w = rR.w - 24, h = (remH > 100 and remH or 600) }
        local subRegions = container.solveDynamicSublayout(currentR, "right-sidebar")
        
        local currentMode = Model.rx.selectedMode:get()
        for id, reg in pairs(subRegions) do
            if widgets[id] then 
                local shouldDraw = true
                if id == "cw" and currentMode ~= "CW" then shouldDraw = false end
                
                if shouldDraw then
                    widgets[id]:draw(id, reg.x, reg.y, reg.w, reg.h, lwc) 
                end
            end
        end
        layout.endRegion()
    end

    -- Debug
    local rD = regions["active-tags"]
    if rD then
        layout.setRegion(rD.x, rD.y, rD.w, rD.h, "active-tags")
        uiInstances.activeTagsViewer:draw("active-tags", rD.x+8, rD.y+8, rD.w-16, rD.h-16, tags, nil)
        layout.endRegion()
    end

    if Edit.isEditModifierHeld(tags) then Edit.drawHandles(mx, my, events.getWidgetAt(mx, my), true) end
    layout.finish(); ui.endFrame(); uiState.endFrame()
end
