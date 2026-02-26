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
local freqEntryBlink = 0

function init()
    print("[Lua] init() called - Version 1.1.0")
    AppState.init()
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
        dispatch.setVfo(state.frequency * 1e6)
        dispatch.setRxActive(state.rxActive)
        hw.setQsdOffset(state.qsdOffsetK)
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
    Edit.init(events)
    layoutOverrides.load()
end

function update(dt)
    frameCount = frameCount + 1
    fpsAccum = fpsAccum + dt; fpsFrames = fpsFrames + 1
    if fpsAccum >= 1.0 then fps = fpsFrames / fpsAccum; fpsAccum = 0; fpsFrames = 0 end
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
        drawText(rT.x + 10, rT.y + 8, string.format("NexRx | %.0f FPS", fps), 0.6, 0.7, 0.9, 1.0)
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
            local bx, by = layout.reserveSpace(50, 26)
            local active = state.selectedMode == m and {"Active"} or {}
            ui.button("mode-"..m:lower(), m, bx, by, 50, 26, {"Mode"..m, table.unpack(active)})
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
        local nV = ui.slider("vol-slider", cx, cy, rL.w - 24, -60, 0, state.volumeDb, nil, "volumeDb")
        if nV ~= state.volumeDb then state.volumeDb = nV end
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
        
        cx, cy = layout.getCursor(); ui.label("ps-l", cx, cy, "Preselector", {"Title"}); layout.newLine(24)
        cx, cy = layout.getCursor(); ui.checkbox("ps-auto", "Auto-tune", cx, cy, state.preselectorAuto, {"PreselAuto"}, "preselectorAuto"); layout.newLine(28)
        cx, cy = layout.getCursor(); ui.checkbox("ps-l1", "Inductor L1", cx, cy, state.preselL1, {"PreselToggle"}, "preselL1"); layout.newLine(28)
        for i = 0, 10 do
            if i % 4 == 0 and i > 0 then layout.newLine(28) end
            if i % 4 == 0 then layout.beginHorizontal(4) end
            local cid = "preselC"..i
            local bx, by = layout.reserveSpace(45, 24)
            ui.checkbox(cid, "C"..i, bx, by, state[cid], {"PreselToggle"}, cid)
            if i % 4 == 3 or i == 10 then layout.endHorizontal() end
        end
        layout.newLine(12)
        cx, cy = layout.getCursor(); ui.label("isg-t", cx, cy, "Int. Signal Gen.", {"Title"}); layout.newLine(24)
        
        -- ISG Frequency Display
        cx, cy = layout.getCursor()
        ui.frequencyDisplay("isg-freq-disp", cx, cy, rR.w - 24, 36, state.isgFrequency, "", {"IsgControl"})
        layout.newLine(44)

        cx, cy = layout.getCursor(); ui.checkbox("isg-en", "Generator Enabled", cx, cy, state.isgEnabled, {"IsgToggle"}, "isgEnabled"); layout.newLine(28)
        cx, cy = layout.getCursor()
        local nbF = ui.slider("isg-fr", cx, cy, rR.w - 24, 0.1, 30.0, state.isgFrequency, {"IsgControl"}, "isgFrequency")
        if nbF ~= state.isgFrequency then state.isgFrequency = nbF end
        layout.newLine(24)
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
