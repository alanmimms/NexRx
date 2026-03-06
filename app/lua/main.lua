--[[
  NexRx Application - Main Lua Entry Point
]]

local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path

local ui = require("ui.widgets")
local uiState = require("ui.state")
local theme = require("ui.theme")
local layout = require("ui.layout")
local container = require("ui.container")
_G.bands = require("bands")
local bands = _G.bands
local dispatch = require("dispatch")
local events = require("events")
local animate = require("animate")
local keys = require("keycodes")
local modeHelper = require("modes")
local smeter = require("smeter")
local AppState = require("app_state")
local Edit = require("edit")
local layoutOverrides = require("layout_overrides")
local Preselector = require("ui.preselector")

local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0
local lastMouseX, lastMouseY = 0, 0
local BUTTON_NAMES = {"LEFT", "MIDDLE", "RIGHT"}

local state = setmetatable({}, {
    __index = function(_, k) return AppState.get(k) end,
    __newindex = function(_, k, v) AppState.set(k, v) end
})

local preselectorWidget = Preselector.new(state)

local function setProperty(name, v)
    local prevTags = setbox.getActiveTags()
    setbox.addTag("prop." .. name)
    local isAnimated = setbox.getBool("animated", false)
    setbox.setActiveTags(prevTags)
    -- print(string.format("[Main] setProperty(%s, %s) animated=%s", name, tostring(v), tostring(isAnimated)))
    if isAnimated then AppState.animateTo(name, v) else AppState.set(name, v) end
end

local wfBins = 512
local wfRows = 256
local spectrumData = {}
local keyStates = {}
local activeTags = {}

local function getAllActiveTags()
    local allTags = {}
    for tagName, _ in pairs(activeTags) do allTags[tagName] = true end
    if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then allTags["input.SHIFT"] = true end
    if activeTags["input.LCTRL"] or activeTags["input.RCTRL"] then allTags["input.CTRL"] = true end
    if activeTags["input.LALT"] or activeTags["input.RALT"] then allTags["input.ALT"] = true end
    if events and events.getModeTags then for _, t in ipairs(events.getModeTags()) do allTags[t] = true end end
    local mx, my = getMousePos()
    local hovered = events.getWidgetAt(mx, my)
    if hovered then
        allTags["state.Hovered"] = true
        if hovered.id then allTags["state.Hovered:" .. hovered.id] = true end
        if hovered.tags then for _, t in ipairs(hovered.tags) do allTags[t] = true end end
    end
    if activeTags["input.MouseLEFT"] and hovered then
        allTags["state.Pressed"] = true
        if hovered.id then allTags["state.Pressed:" .. hovered.id] = true end
    end
    return allTags
end

local hwConnected = false
local freqEntryText = ""
local isgFreqEntryText = ""
local freqEntryBlink = 0

function init()
    print("[Lua] init() called - Version 1.1.0")
    wfBins = math.floor(state.wfBins or 512)
    wfRows = math.floor(state.wfRows or 256)
    for i = 1, wfBins do spectrumData[i] = -100 end
    local configFiles = {"config/bands.lua", "config/colormaps.lua", "config/events.lua", "config/layout.lua", "config/modes.lua", "config/constraints.lua"}
    for _, file in ipairs(configFiles) do setbox.loadFile(file) end
    setClearColor(0.1, 0.1, 0.15)
    if audio.isInitialized() then audio.start() end
    if waterfall.init(wfBins, wfRows) then
        waterfall.setRange(state.wfMinDb, state.wfMaxDb)
        if colormaps and colormaps[state.wfColormap] then waterfall.setColormapData(colormaps[state.wfColormap]) end
        dispatch.enableWaterfall()
    end
    bands.init()
    events.init()
    layout.setEventsModule(events)
    ui.setEventsModule(events)
    ui.setLayoutModule(layout)
    
    if hw.connect("127.0.0.1", 5000, 5001) then
        hwConnected = true
        dispatch.enableHardware()
        -- Initialize AppState AFTER connection so setters can send commands
        AppState.init()
    else
        AppState.init()
    end
    events.registerHandler("vfo_control", function(e, w, p)
        local delta = e.delta or (e.key == "RIGHT" and 1 or (e.key == "LEFT" and -1 or 0))
        local prop = p.property or "frequency"
        if delta ~= 0 and p and p.step then 
            local newVal = state[prop] + delta * p.step
            -- print(string.format("[Main] vfo_control: %s -> %s", prop, tostring(newVal)))
            setProperty(prop, newVal); return true 
        end
        return false
    end)
    events.registerHandler("toggle_property", function(e, w, p)
        local prop = p.property or (w and w.data and w.data.property)
        if prop then setProperty(prop, not state[prop]); return true end
        return false
    end)
    events.registerHandler("slider_activate", function(e, w, p)
        if w and w.id then uiState.setActive(w.id); return true end
        return false
    end)
    events.registerHandler("slider_adjust", function(e, w, p)
        local prop = p.property or (w and w.data and w.data.property)
        if not prop then return false end
        local val = state[prop]
        local step = p.step or (w and w.data and (w.data.max - w.data.min) * 0.01) or 0.01
        if e.delta then 
            local newVal = val + e.delta * step
            -- print(string.format("[Main] slider_adjust: %s -> %s", prop, tostring(newVal)))
            setProperty(prop, newVal); return true 
        end
        return false
    end)
    events.registerHandler("set_value", function(e, w, p)
        if p and p.property and p.value ~= nil then setProperty(p.property, p.value); return true end
        return false
    end)

    -- =======================================================================
    -- Frequency Entry Handlers
    -- =======================================================================
    
    events.registerHandler("freq_entry_start", function(event, widget)
        if widget and widget.tags and table.concat(widget.tags, ","):find("IsgControl") then
            isgFreqEntryText = ""
            events.addModeTag("IsgFreqEntryMode")
            events.addModeTag("FreqEntryMode") -- Both for global rules
        else
            freqEntryText = ""
            events.addModeTag("FreqEntryMode")
        end
        return true
    end)

    events.registerHandler("freq_entry_cancel", function(event, widget)
        events.removeModeTag("FreqEntryMode")
        events.removeModeTag("IsgFreqEntryMode")
        freqEntryText = ""
        isgFreqEntryText = ""
        return true
    end)

    events.registerHandler("freq_entry_backspace", function(event, widget)
        if events.hasModeTag("IsgFreqEntryMode") then
            if #isgFreqEntryText > 0 then isgFreqEntryText = isgFreqEntryText:sub(1, -2) end
        elseif events.hasModeTag("FreqEntryMode") then
            if #freqEntryText > 0 then freqEntryText = freqEntryText:sub(1, -2) end
        end
        return true
    end)

    events.registerHandler("freq_entry_text", function(event, widget)
        local text = event.text or ""
        for i = 1, #text do
            local ch = text:sub(i, i)
            if ch:match("[0-9]") or (ch == ".") then
                if events.hasModeTag("IsgFreqEntryMode") then
                    if ch ~= "." or not isgFreqEntryText:find("%.") then
                        isgFreqEntryText = isgFreqEntryText .. ch
                    end
                elseif events.hasModeTag("FreqEntryMode") then
                    if ch ~= "." or not freqEntryText:find("%.") then
                        freqEntryText = freqEntryText .. ch
                    end
                end
            end
        end
        return true
    end)

    events.registerHandler("freq_entry_confirm", function(event, widget)
        if events.hasModeTag("IsgFreqEntryMode") then
            local newFreq = tonumber(isgFreqEntryText)
            if newFreq and newFreq >= 0.1 and newFreq <= 30.0 then
                setProperty("isgFrequency", newFreq)
            end
            events.removeModeTag("IsgFreqEntryMode")
            events.removeModeTag("FreqEntryMode")
            isgFreqEntryText = ""
        elseif events.hasModeTag("FreqEntryMode") then
            local newFreq = tonumber(freqEntryText)
            if newFreq and newFreq >= 0.1 and newFreq <= 30.0 then
                setProperty("frequency", newFreq)
                if state.activeVFO == "A" then AppState.set("vfoA", newFreq) else AppState.set("vfoB", newFreq) end
                bands.setFrequency(newFreq * 1e6)
            end
            events.removeModeTag("FreqEntryMode")
            freqEntryText = ""
        end
        return true
    end)

    Edit.init(events)
    layoutOverrides.load()
end

function update(dt)
    frameCount = frameCount + 1
    fpsAccum = fpsAccum + dt; fpsFrames = fpsFrames + 1
    if fpsAccum >= 1.0 then 
        fps = fpsFrames / fpsAccum; 
        fpsAccum = 0; 
        fpsFrames = 0 
        if diag and diag.logLevels then diag.logLevels() end
    end
    animate.update(dt)
    freqEntryBlink = (freqEntryBlink + dt) % 1.0
    for _, sc in ipairs(keys.getAllScancodes()) do local n = keys.getName(sc); if n then activeTags["input."..n] = isKeyDown(sc) or nil end end
    activeTags["input.SHIFT"] = isShiftDown() or nil
    activeTags["input.CTRL"] = isCtrlDown() or nil
    activeTags["input.ALT"] = isAltDown() or nil
    activeTags["input.MouseLEFT"] = isMouseDown(0) or nil
    activeTags["input.MouseMIDDLE"] = isMouseDown(1) or nil
    activeTags["input.MouseRIGHT"] = isMouseDown(2) or nil
    local mx, my = getMousePos()
    local tags = getAllActiveTags()
    uiState.beginFrame()
    local wheel = getMouseWheel()
    if wheel ~= 0 then events.dispatch(events.createEvent(events.Type.MOUSE_WHEEL, {x=mx,y=my,delta=wheel,modifiers=tags})) end
    for b = 0, 2 do
        if isMouseClicked(b) then events.dispatch(events.createEvent(events.Type.MOUSE_DOWN, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=tags}))
        elseif isMouseReleased(b) then events.dispatch(events.createEvent(events.Type.MOUSE_UP, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=tags})) end
    end
    if mx ~= lastMouseX or my ~= lastMouseY then
        events.dispatch(events.createEvent(events.Type.MOUSE_MOVE, {x=mx,y=my,dx=mx-lastMouseX,dy=my-lastMouseY,modifiers=tags}))
        lastMouseX, lastMouseY = mx, my
    end
    if hwConnected then
        local hwSpec = hw.getSpectrum()
        if hwSpec and #hwSpec > 0 then 
            if #hwSpec ~= wfBins then
                wfBins = #hwSpec
                waterfall.init(wfBins, wfRows)
                waterfall.setRange(state.wfMinDb, state.wfMaxDb)
                if colormaps and colormaps[state.wfColormap] then waterfall.setColormapData(colormaps[state.wfColormap]) end
            end
            spectrumData = hwSpec 
            dispatch.updateWaterfall(spectrumData)
        end
    else
        dispatch.updateWaterfall(spectrumData)
    end
end

function draw()
    local winW, winH = getWindowSize()
    local mx, my = getMousePos()
    events.clearWidgets()
    ui.beginFrame()
    layout.begin(0, 0, winW, winH)
    local regions = container.solve(winW, winH)
    if not regions then return end

    -- Top Bar
    local rT = regions["top-bar"]
    if rT then
        layout.setRegion(rT.x, rT.y, rT.w, rT.h, "top-bar")
        events.registerWidget("top-bar", rT, {"widget.StatusBar"})
        drawRect(rT.x, rT.y, rT.w, rT.h, 0.08, 0.08, 0.12, 1.0)
        local frameCount = hwConnected and hw.getFramesReceived and hw.getFramesReceived() or 0
        drawText(rT.x + 10, rT.y + 8, string.format("NexRx | %.0f FPS | Frames: %d", fps, frameCount), 0.6, 0.7, 0.9, 1.0)
        layout.endRegion()
    end

    -- Left Sidebar
    local rL = regions["left-sidebar"]
    if rL then
        layout.setRegion(rL.x, rL.y, rL.w, rL.h, "left-sidebar")
        ui.panel("left-sidebar", rL.x, rL.y, rL.w, rL.h, {"Sidebar"})
        layout.pad(12)
        local cx, cy = layout.getCursor()
        ui.frequencyDisplay("freq-disp", cx, cy, rL.w - 24, 36, state.frequency, freqEntryText)
        layout.newLine(44)
        
        cx, cy = layout.getCursor()
        local nF = ui.slider("freq-slider", cx, cy, rL.w - 24, 0.1, 30.0, state.frequency, {"VFOControl"}, "frequency")
        if nF ~= state.frequency then state.frequency = nF end
        layout.newLine(24)

        cx, cy = layout.getCursor(); ui.label("mode-l", cx, cy, "Mode", {"Title"}); layout.newLine(24)
        layout.beginHorizontal(4)
        for _, m in ipairs(modeHelper.names) do
            local bx, by = layout.reserveSpace(40, 26)
            local active = state.selectedMode == m and {"state.Active"} or {}
            local label = m == "BYPASS" and "RAW" or m
            if ui.button("mode-"..m:lower(), label, bx, by, 40, 26, {"Mode"..m, table.unpack(active)}) then
                setProperty("selectedMode", m)
            end
        end
        layout.endHorizontal(); layout.newLine(12)

        cx, cy = layout.getCursor(); ui.label("band-l", cx, cy, "Band", {"Title"}); layout.newLine(24)
        layout.beginHorizontal(4)
        for _, b in ipairs({"160m","80m","40m","20m","15m","10m"}) do
            local bx, by = layout.reserveSpace(40, 26)
            local active = state.selectedBand == b and {"Active"} or {}
            ui.button("band-"..b, b, bx, by, 40, 26, {"Band"..b, table.unpack(active)})
        end
        layout.endHorizontal(); layout.newLine(24)

        cx, cy = layout.getCursor(); ui.label("vol-l", cx, cy, "Volume", {"Title"}); layout.newLine(24)
        cx, cy = layout.getCursor()
        local nV = ui.slider("vol-slider", cx, cy, rL.w - 24, -60, 20, state.volumeDb, nil, "volumeDb")
        if nV ~= state.volumeDb then state.volumeDb = nV end
        layout.newLine(24)

        cx, cy = layout.getCursor(); ui.label("rfg-l", cx, cy, "RF Gain", {"Title"}); layout.newLine(24)
        cx, cy = layout.getCursor()
        local nRG = ui.slider("rfg-slider", cx, cy, rL.w - 24, -20, 60, state.rfGainDb, nil, "rfGainDb")
        if nRG ~= state.rfGainDb then state.rfGainDb = nRG end
        layout.newLine(24)
        layout.endRegion()
    end

    -- Center Area
    local rC = regions["center-area"]
    if rC then
        ui.panel("center-panel", rC.x, rC.y, rC.w, rC.h)
        local sub = container.solveSublayout(rC, {
            { id = "spec", tags = {"widget.Spectrum"},  anchor = "top", group = "center-col" },
            { id = "wf",   tags = {"widget.Waterfall"}, anchor = "top", group = "center-col" },
        })
        if sub["spec"] then
            local rs = sub["spec"]
            layout.setRegion(rs.x, rs.y, rs.w, rs.h, "spec")
            dispatch.renderSpectrum(spectrumData, rs.x, rs.y, rs.w, rs.h)
            
            -- Draw Legend (Units for horizontal and vertical grid squares)
            ui.graticuleLegend("spec-legend", rs.x + 10, rs.y + 10, 100, 45, "9.6 kHz/div", "20 dB/div")
            
            events.registerWidget("spec", rs, {"widget.Spectrum", "widget.VFOControl"})
            layout.endRegion()
        end
        if sub["wf"] then
            local rw = sub["wf"]
            layout.setRegion(rw.x, rw.y, rw.w, rw.h, "wf")
            dispatch.renderWaterfall(rw.x, rw.y, rw.w, rw.h)
            events.registerWidget("wf", rw, {"widget.Waterfall", "widget.VFOControl"})
            layout.endRegion()
        end
    end

    -- Right Sidebar
    local rR = regions["right-sidebar"]
    if rR then
        layout.setRegion(rR.x, rR.y, rR.w, rR.h, "right-sidebar")
        ui.panel("right-sidebar", rR.x, rR.y, rR.w, rR.h, {"Sidebar"})
        layout.pad(12)
        local cx, cy = layout.getCursor()
        ui.toggle("rx-toggle", state.rxActive and "RX ON" or "RX OFF", cx, cy, rR.w - 24, 40, state.rxActive, {"RxToggle"})
        layout.newLine(52)
        cx, cy = layout.getCursor(); ui.label("sm-l", cx, cy, "S-Meter", {"Title"}); layout.newLine(24)
        cx, cy = layout.getCursor(); ui.smeter("s-meter", cx, cy, rR.w - 24, 28, smeter.getReading()); layout.newLine(48)
        
        -- AGC Mode
        cx, cy = layout.getCursor(); ui.label("agc-l", cx, cy, "AGC Mode", {"Title"}); layout.newLine(24)
        layout.beginHorizontal(2)
        local modes = {"Off", "Fast", "Med", "Slow"}
        for i, m in ipairs(modes) do
            if i == 3 then layout.endHorizontal(); layout.newLine(28); layout.beginHorizontal(2) end
            local bx, by = layout.reserveSpace((rR.w-36)/2, 24)
            local buttonTags = {"AgcMode"}
            if state.agcMode == (i-1) then table.insert(buttonTags, "state.Active") end
            if ui.button("agc-"..m:lower(), m, bx, by, (rR.w-36)/2, 24, buttonTags) then
                setProperty("agcMode", i-1)
            end
        end
        layout.endHorizontal(); layout.newLine(24)

        -- Preselector Section
        local psW = rR.w - 24
        local psH = 220
        local psX, psY = layout.reserveSpace(psW, psH)
        preselectorWidget:draw("preselector-main", psX, psY, psW, psH)
        layout.newLine(12)

        cx, cy = layout.getCursor(); ui.label("isg-t", cx, cy, "Int. Signal Gen.", {"Title"}); layout.newLine(24)
        
        -- ISG Frequency Display
        cx, cy = layout.getCursor()
        ui.frequencyDisplay("isg-freq-disp", cx, cy, rR.w - 24, 36, state.isgFrequency, isgFreqEntryText, {"IsgControl"})
        layout.newLine(44)

        cx, cy = layout.getCursor(); ui.checkbox("isg-en", "Generator Enabled", cx, cy, state.isgEnabled, {"IsgToggle"}, "isgEnabled"); layout.newLine(28)
        cx, cy = layout.getCursor()
        local nbF = ui.slider("isg-fr", cx, cy, rR.w - 24, 0.1, 30.0, state.isgFrequency, {"IsgControl"}, "isgFrequency")
        if nbF ~= state.isgFrequency then state.isgFrequency = nbF end
        layout.newLine(32)

        -- Audio Utilities
        cx, cy = layout.getCursor(); ui.label("aud-t", cx, cy, "Audio Utilities", {"Title"}); layout.newLine(24)
        cx, cy = layout.getCursor(); ui.checkbox("mut-en", "Master Mute", cx, cy, state.muteEnabled, {"MuteToggle"}, "muteEnabled"); layout.newLine(28)
        cx, cy = layout.getCursor(); ui.checkbox("flt-en", "Demod Filters", cx, cy, state.demodFilterEnabled, {"FilterToggle"}, "demodFilterEnabled"); layout.newLine(28)
        cx, cy = layout.getCursor(); ui.checkbox("tst-en", "440Hz Test Tone", cx, cy, state.testToneEnabled, {"TestToneToggle"}, "testToneEnabled"); layout.newLine(28)
        
        layout.endRegion()
    end

    -- Debug
    local rD = regions["active-tags"]
    if rD then
        layout.setRegion(rD.x, rD.y, rD.w, rD.h, "active-tags")
        ui.activeTagsViewer("at-v", rD.x+8, rD.y+8, rD.w-16, rD.h-16, getAllActiveTags())
        layout.endRegion()
    end

    if Edit.isEditModifierHeld(getAllActiveTags()) then Edit.drawHandles(mx, my, events.getWidgetAt(mx, my), true) end
    layout.finish(); ui.endFrame(); uiState.endFrame()
end
