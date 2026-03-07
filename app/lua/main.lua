--[[
  NexRx Application - Main Lua Entry Point
]]

local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path

local ui = require("ui.Widgets")
local uiState = require("ui.State")
local layout = require("ui.Layout")
local container = require("ui.Container")
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

-- Modular Widget Classes
local Preselector = require("ui.Preselector")
local ISG = require("ui.ISG")
local AGC = require("ui.AGC")
local AudioUtils = require("ui.AudioUtils")
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

local state = setmetatable({}, {
    __index = function(_, k) return AppState.get(k) end,
    __newindex = function(_, k, v) AppState.set(k, v) end
})

-- Sidebar modular widgets
local widgets = {
    presel = Preselector.new(state),
    isg = ISG.new(state),
    agc = AGC.new(state),
    audio = AudioUtils.new(state)
}

-- Persistent widget instances for main UI
local uiInstances = {}

local function setProperty(name, v)
    local prevTags = setbox.getActiveTags()
    setbox.addTag("prop." .. name)
    local isAnimated = setbox.getBool("animated")
    setbox.setActiveTags(prevTags)
    if isAnimated then AppState.animateTo(name, v) else AppState.set(name, v) end
end

local function getAllActiveTags()
    local allTags = {}
    local globalTags = setbox.getActiveTags() -- Use setbox internal tags
    for _, tagName in ipairs(globalTags) do allTags[tagName] = true end

    if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then allTags["input.SHIFT"] = true end
    if allTags["input.LCTRL"] or allTags["input.RCTRL"] then allTags["input.CTRL"] = true end
    if allTags["input.LALT"] or allTags["input.RALT"] then allTags["input.ALT"] = true end
    
    if events and events.getModeTags then for _, t in ipairs(events.getModeTags()) do allTags[t] = true end end
    
    local mx, my = getMousePos()
    local hovered = events.getWidgetAt(mx, my)
    if hovered and hovered.id then
        allTags["state.Hovered:" .. hovered.id] = true
        if hovered.tags then for _, t in ipairs(hovered.tags) do allTags[t] = true end end
    end
    
    if allTags["input.MouseLEFT"] and hovered and hovered.id then
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
local freqEntryBlink = 0
local wfBins = 512
local wfRows = 256
local spectrumData = {}

function init()
    AppState.init()
    
    wfBins = math.floor(state.wfBins or 512)
    wfRows = math.floor(state.wfRows or 256)
    for i = 1, wfBins do spectrumData[i] = -100 end
    
    setClearColor(0.1, 0.1, 0.15)
    if audio.isInitialized() then audio.start() end
    if waterfall.init(wfBins, wfRows) then
        waterfall.setRange(state.wfMinDb, state.wfMaxDb)
        if _G.colormaps and _G.colormaps[state.wfColormap] then 
            waterfall.setColormapData(_G.colormaps[state.wfColormap]) 
        end
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
    end

    -- Create UI Instances
    uiInstances.leftPanel = Panel.new()
    uiInstances.rightPanel = Panel.new()
    uiInstances.centerPanel = Panel.new()
    uiInstances.freqDisplay = FrequencyDisplay.new()
    uiInstances.freqSlider = Slider.new({ onAdjust = function(v) AppState.set("frequency", v) end })
    uiInstances.volumeSlider = Slider.new({ onAdjust = function(v) AppState.set("volumeDb", v) end })
    uiInstances.rfGainSlider = Slider.new({ onAdjust = function(v) AppState.set("rfGainDb", v) end })
    uiInstances.smeter = SMeter.new()
    uiInstances.activeTagsViewer = ActiveTags.new()
    uiInstances.graticuleLegend = GraticuleLegend.new()
    uiInstances.rxToggle = Button.new({ onClick = function() AppState.set("rxActive", not state.rxActive) end })
    
    uiInstances.modeLabels = Label.new()
    uiInstances.bandLabels = Label.new()
    uiInstances.volLabel = Label.new()
    uiInstances.rfGainLabel = Label.new()
    uiInstances.smeterLabel = Label.new()

    uiInstances.modeButtons = {}
    for _, m in ipairs(modeHelper.names) do
        uiInstances.modeButtons[m] = Button.new({ onClick = function() AppState.set("selectedMode", m) end })
    end

    uiInstances.bandButtons = {}
    local bandList = {"160m","80m","40m","20m","15m","10m"}
    for _, b in ipairs(bandList) do
        uiInstances.bandButtons[b] = Button.new({ onClick = function() AppState.set("selectedBand", b) end })
    end

    events.registerHandler("vfo_control", function(e, w, p)
        local delta = e.delta or (e.key == "RIGHT" and 1 or (e.key == "LEFT" and -1 or 0))
        local prop = p.property or "frequency"
        if delta ~= 0 and p and p.step then 
            local newVal = state[prop] + delta * p.step
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
            setProperty(prop, newVal); return true 
        end
        return false
    end)
    events.registerHandler("set_value", function(e, w, p)
        if p and p.property and p.value ~= nil then setProperty(p.property, p.value); return true end
        return false
    end)

    -- Frequency Entry Handlers
    events.registerHandler("freq_entry_start", function(event, widget)
        if widget and widget.tags and table.concat(widget.tags, ","):find("IsgControl") then
            isgFreqEntryText = ""
            events.addModeTag("IsgFreqEntryMode"); events.addModeTag("FreqEntryMode")
        else
            freqEntryText = ""
            events.addModeTag("FreqEntryMode")
        end
        return true
    end)

    events.registerHandler("freq_entry_cancel", function(event, widget)
        events.removeModeTag("FreqEntryMode"); events.removeModeTag("IsgFreqEntryMode")
        freqEntryText = ""; isgFreqEntryText = ""; return true
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
                    if ch ~= "." or not isgFreqEntryText:find("%.") then isgFreqEntryText = isgFreqEntryText .. ch end
                elseif events.hasModeTag("FreqEntryMode") then
                    if ch ~= "." or not freqEntryText:find("%.") then freqEntryText = freqEntryText .. ch end
                end
            end
        end
        return true
    end)

    events.registerHandler("freq_entry_confirm", function(event, widget)
        if events.hasModeTag("IsgFreqEntryMode") then
            local newFreq = tonumber(isgFreqEntryText)
            if newFreq and newFreq >= 0.1 and newFreq <= 30.0 then setProperty("isgFrequency", newFreq * 1e6) end
            events.removeModeTag("IsgFreqEntryMode"); events.removeModeTag("FreqEntryMode"); isgFreqEntryText = ""
        elseif events.hasModeTag("FreqEntryMode") then
            local newFreq = tonumber(freqEntryText)
            if newFreq and newFreq >= 0.1 and newFreq <= 30.0 then
                setProperty("frequency", newFreq * 1e6)
                if state.activeVFO == "A" then AppState.set("vfoA", newFreq * 1e6) else AppState.set("vfoB", newFreq * 1e6) end
                bands.setFrequency(newFreq * 1e6)
            end
            events.removeModeTag("FreqEntryMode"); freqEntryText = ""
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
        fps = fpsFrames / fpsAccum; fpsAccum = 0; fpsFrames = 0 
    end
    animate.update(dt)
    freqEntryBlink = (freqEntryBlink + dt) % 1.0
    
    local scancodes = keys.getAllScancodes()
    for _, sc in ipairs(scancodes) do
        local n = keys.getName(sc)
        if n then
            activeTags["input."..n] = isKeyDown(sc) or nil
        end
    end
    
    activeTags["input.SHIFT"] = isShiftDown() or nil
    activeTags["input.CTRL"] = isCtrlDown() or nil
    activeTags["input.ALT"] = isAltDown() or nil
    activeTags["input.MouseLEFT"] = isMouseDown(0) or nil
    activeTags["input.MouseMIDDLE"] = isMouseDown(1) or nil
    activeTags["input.MouseRIGHT"] = isMouseDown(2) or nil

    local mx, my = getMousePos()
    local tags = getAllActiveTags()
    uiState.beginFrame()
    setbox.setActiveTags(tags)
    
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
        if hwSpec and #hwSpec > 0 then spectrumData = hwSpec; dispatch.updateWaterfall(spectrumData) end
    else
        dispatch.updateWaterfall(spectrumData)
    end
end

function draw()
    local winW, winH = getWindowSize()
    local mx, my = getMousePos()
    local tags = getAllActiveTags()
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
        local fC = hwConnected and hw.getFramesReceived and hw.getFramesReceived() or 0
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
        uiInstances.freqDisplay:draw("freq-disp", cx, cy, rL.w - 24, 36, state.frequency, freqEntryText, {"VFOControl"}, lwc)
        layout.newLine(44)
        
        cx, cy = layout.getCursor()
        uiInstances.freqSlider:draw("freq-slider", cx, cy, rL.w - 24, 0.1e6, 30.0e6, state.frequency, lwc)
        layout.newLine(32)

        cx, cy = layout.getCursor(); 
        uiInstances.modeLabels:draw("mode-label", cx, cy, lwc); 
        layout.newLine(24)
        
        layout.beginHorizontal(0)
        for _, m in ipairs(modeHelper.names) do
            local bx, by = layout.reserveSpace(40, 26)
            local active = state.selectedMode == m and {"state.Active"} or {}
            local btn = uiInstances.modeButtons[m]
            -- btn.label will be resolved from rule matching id.mode-USB etc
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
            local active = state.selectedBand == b and {"state.Active"} or {}
            uiInstances.bandButtons[b]:draw("band-"..b, bx, by, 40, 26, active, lwc)
            if i < #bandList then layout.space(4) end
        end
        layout.endHorizontal(); layout.newLine(32)

        cx, cy = layout.getCursor(); 
        uiInstances.volLabel:draw("vol-label", cx, cy, lwc); 
        layout.newLine(24)
        cx, cy = layout.getCursor()
        uiInstances.volumeSlider:draw("volume-slider", cx, cy, rL.w - 24, -60, 0, state.volumeDb, lwc)
        layout.newLine(32)

        cx, cy = layout.getCursor(); 
        uiInstances.rfGainLabel:draw("rfg-label", cx, cy, lwc); 
        layout.newLine(24)
        cx, cy = layout.getCursor()
        uiInstances.rfGainSlider:draw("rfg-slider", cx, cy, rL.w - 24, -20, 60, state.rfGainDb, lwc)
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
            dispatch.renderSpectrum(spectrumData, rs.x, rs.y, rs.w, rs.h)
            uiInstances.graticuleLegend:draw("spec-legend", rs.x + 10, rs.y + 10, 100, 45, "9.6 kHz/div", "20 dB/div")
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
        local lwc = setbox.newContext({"widget.Sidebar", "widget.RightSidebar", "id.right-sidebar"})
        layout.setRegion(rR.x, rR.y, rR.w, rR.h, "right-sidebar")
        uiInstances.rightPanel:draw("right-sidebar", rR.x, rR.y, rR.w, rR.h, lwc)
        layout.pad(12)
        
        cx, cy = layout.getCursor()
        uiInstances.rxToggle:draw("rx-toggle", cx, cy, rR.w - 24, 40, state.rxActive and {"state.Active"} or {}, lwc)
        layout.newLine(52)
        
        cx, cy = layout.getCursor(); 
        uiInstances.smeterLabel:draw("sm-label", cx, cy, lwc); 
        layout.newLine(24)
        
        cx, cy = layout.getCursor(); 
        uiInstances.smeter:draw("s-meter", cx, cy, rR.w - 24, 28, smeter.getReading(), lwc); 
        layout.newLine(48)
        
        cx, cy = layout.getCursor()
        local remH = (rR.y + rR.h) - cy - 12
        local currentR = { x = rR.x + 12, y = cy, w = rR.w - 24, h = (remH > 100 and remH or 600) }
        local subRegions = container.solveDynamicSublayout(currentR, "right-sidebar")
        
        for id, reg in pairs(subRegions) do
            if widgets[id] then 
                widgets[id]:draw(id, reg.x, reg.y, reg.w, reg.h, lwc) 
            end
        end
        layout.endRegion()
    end

    -- Debug
    local rD = regions["active-tags"]
    if rD then
        layout.setRegion(rD.x, rD.y, rD.w, rD.h, "active-tags")
        uiInstances.activeTagsViewer:draw("at-v", rD.x+8, rD.y+8, rD.w-16, rD.h-16, tags, nil)
        layout.endRegion()
    end

    if Edit.isEditModifierHeld(tags) then Edit.drawHandles(mx, my, events.getWidgetAt(mx, my), true) end
    layout.finish(); ui.endFrame(); uiState.endFrame()
end
