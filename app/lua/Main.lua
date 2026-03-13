--[[
  NexRx Application - Main Lua Entry Point
]]

local basePath = "lua/"
package.path = basePath .. "?.lua;" .. basePath .. "?/init.lua;" .. package.path
io.stdout:setvbuf("no")
print("[Main] Script loading...")

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

-- Modular Widget Classes
local Preselector = require("ui.Preselector")
local ISG = require("ui.ISG")
local AGC = require("ui.AGC")
local AudioUtils = require("ui.AudioUtils")
local CWUtils = require("ui.CWUtils")
local Panel = require("ui.Panel")
local SMeterWidget = require("ui.SMeter") -- Renamed to avoid collision with smeter helper
local ActiveTags = require("ui.ActiveTags")
local GraticuleLegend = require("ui.GraticuleLegend")
local SignalBox = require("ui.SignalBox")
local Button = require("ui.Button")
local Slider = require("ui.Slider")
local FrequencyDisplay = require("ui.FrequencyDisplay")
local Label = require("ui.Label")

local frameCount = 0
local fps = 0
local fpsAccum = 0
local fpsFrames = 0
local lastMouseX, lastMouseY = 0, 0
local BUTTON_NAMES = {"LEFT", "MIDDLE", "RIGHT"}
local keyStates = {}
local activeTags = {}

-- Persistent instances
local uiInstances = {}
local widgets = {} -- Initialized in init()

local function getAllActiveTags()
    local allTags = {}
    if activeTags["input.LSHIFT"] or activeTags["input.RSHIFT"] then allTags["input.SHIFT"] = true end
    if activeTags["input.LCTRL"] or activeTags["input.RCTRL"] then allTags["input.CTRL"] = true end
    if activeTags["input.LALT"] or activeTags["input.RALT"] then allTags["input.ALT"] = true end
    for k, v in pairs(activeTags) do if v then allTags[k] = true end end
    if events and events.getModeTags then for _, t in ipairs(events.getModeTags()) do allTags[t] = true end end
    local mx, my = getMousePos()
    local hovered = events.getWidgetAt(mx, my)
    if hovered and hovered.id then allTags["state.Hovered:" .. hovered.id] = true end
    if activeTags["input.MouseLEFT"] and hovered and hovered.id then allTags["state.Pressed:" .. hovered.id] = true end
    local list = {}; for tag, _ in pairs(allTags) do table.insert(list, tag) end
    table.sort(list); return list
end

print("[Main] Loading configurations...")
local configFiles = { "config/default.lua", "config/settings.lua", "config/bands.lua", "config/colormaps.lua", "config/events.lua", "config/constraints.lua" }
for _, file in ipairs(configFiles) do setbox.loadFile(file) end

local spectrumData = {}
_G.rxStats = { rms=0, gain0=0, phase0=0, gain1=0, phase1=0, w0_mag=0, w1_mag=0 }
_G.isgFreqEntryText = ""; _G.isgFreqEntryCursor = 0
_G.freqEntryText = ""; _G.freqEntryCursor = 0

local function clamp(v, min, max) return math.max(min, math.min(max, v)) end

function init()
    print("[Main] init() starting...")
    
    print("  [Main] AppController.init...")
    AppController.init()
    print("  [Main] calibration.init...")
    _G.calibration.init()
    
    local wfBins = 1024; local wfRows = 256; for i = 1, wfBins do spectrumData[i] = -100 end
    setClearColor(0.1, 0.1, 0.15)
    
    if audio.isInitialized() then 
        print("  [Main] audio.start...")
        audio.start() 
    end
    
    if waterfall.init(wfBins, wfRows) then
        print("  [Main] waterfall.init success")
        waterfall.setRange(Model.waterfall.minDB:get(), Model.waterfall.maxDB:get())
        if _G.colormaps and _G.colormaps[Model.waterfall.colormap:get()] then waterfall.setColormapData(_G.colormaps[Model.waterfall.colormap:get()]) end
        Hardware.enableWaterfall()
    end
    
    print("  [Main] bands/events/layout init...")
    bands.init(); events.init(); layout.setEventsModule(events); ui.setEventsModule(events); ui.setLayoutModule(layout)
    
    if hw.connect("127.0.0.1", 5000, 5001) then 
        print("  [Main] hw.connect success")
        Hardware.enableHardware() 
    end
    
    print("  [Main] Instantiating widgets...")
    uiInstances.leftPanel = Panel.new(); uiInstances.rightPanel = Panel.new(); uiInstances.centerPanel = Panel.new(); uiInstances.tagsPanel = Panel.new()
    
    -- Initialize widget table inside init()
    widgets = {
        ["id-smeter"] = SMeterWidget.new(),
        ["id-cal"] = Button.new({ onClick = function() hw.calibrate() end }),
        ["id-rx-freq"] = FrequencyDisplay.new({ valueObs = Model.rx.VFO.activeValue }),
        ["id-rx-slider"] = Slider.new({ valueObs = Model.rx.VFO.activeValue }),
        ["id-rx-r1k"] = Button.new({ label = "<1k>", onClick = function() Model.roundFrequency("rx.VFO.activeValue", 1000) end }),
        ["id-rx-r100"] = Button.new({ label = "<00>", onClick = function() Model.roundFrequency("rx.VFO.activeValue", 100) end }),
        ["id-rx-vol"] = Slider.new({ valueObs = Model.rx.volume.DB, propertyName = "rx.volume.DB" }),
        ["id-rx-gain"] = Slider.new({ valueObs = Model.rx.RF.gainDB, propertyName = "rx.RF.gainDB" }),
    }

    local modes = {"LSB", "USB", "AM", "CW", "FM"}
    for _, m in ipairs(modes) do
        widgets["id-rx-mode-" .. m] = Button.new({ 
            getText = function() return m end, 
            onClick = function() Model.set("rx.selectedMode", m) end 
        })
    end

    local bandsList = {"160m","80m","40m","20m","15m","10m"}
    for _, b in ipairs(bandsList) do
        widgets["id-rx-band-" .. b] = Button.new({ 
            getText = function() return b end, 
            onClick = function() Model.set("rx.selectedBand", b) end 
        })
    end

    widgets["id-spec"] = { 
        graticuleLegend = GraticuleLegend.new(),
        signalBox = SignalBox.new(),
        draw = function(self, id, x, y, w, h, parentLWC)
            Hardware.renderSpectrum(spectrumData, x, y, w, h)
            local currentZoom = Model.waterfall.zoom:get() or 1.0
            local span = 96000 / currentZoom
            local hScale = span / 10
            local hScaleStr = string.format("%.1f kHz/div", hScale / 1000)
            if hScale < 1000 then hScaleStr = string.format("%.0f Hz/div", hScale) end
            self.graticuleLegend:draw("spec-legend", x + 10, y + 10, 100, 45, parentLWC, hScaleStr, "20 dB/div")
            events.registerWidget("id-spec", {x=x, y=y, w=w, h=h}, {"widget.Spectrum", "widget.VFOControl"})
            -- Render SignalBoxes
            local center = Model.spectrumCenterFreq:get()
            local startF = center - span/2
            local selectedIdx = Model.selectedSignalBoxIndex:get()
            local boxes = Model.signalBoxes:get()
            for i, box in ipairs(boxes) do
                local bw = box.bandwidth; local freq = box.frequency; local mode = box.mode
                local fLow, fHigh = freq, freq
                if mode == "USB" then fHigh = freq + bw elseif mode == "LSB" then fLow = freq - bw
                elseif mode == "AM" then fLow, fHigh = freq - bw, freq + bw
                elseif mode == "CW" then local pitch = Model.rx.CW.pitch:peek() or 600; fLow, fHigh = freq + pitch - 500, freq + pitch + 500 end
                local x1 = x + (fLow - startF) / span * w; local x2 = x + (fHigh - startF) / span * w
                local bx = x1; local bw_px = x2 - x1
                local boxTags = {}; if i == selectedIdx then table.insert(boxTags, "state.Selected") end
                if box.ghost then table.insert(boxTags, "state.Ghost") end
                self.signalBox:draw("sb"..i, bx, y, bw_px, h, parentLWC, tostring(box.id), boxTags)
            end
        end
    }
    
    widgets["id-wf"] = { draw = function(self, id, x, y, w, h, parentLWC)
        local zoom = Model.waterfall.zoom:get()
        Hardware.renderWaterfall(x, y, w, h, zoom, 0.5)
        events.registerWidget("id-wf", {x=x, y=y, w=w, h=h}, {"widget.Waterfall", "widget.VFOControl"})
    end }
    
    widgets["id-isg"] = ISG.new({ ISG = Model.ISG })
    widgets["id-presel"] = Preselector.new({ preselector = Model.preselector })
    widgets["id-cw"] = CWUtils.new({ rx = Model.rx })
    widgets["id-agc"] = AGC.new({ AGC = Model.rx.AGC })
    widgets["id-audio"] = AudioUtils.new({ rx = Model.rx })
    widgets["id-active-tags"] = ActiveTags.new()

    print("  [Main] Edit.init...")
    Edit.init(events)
    print("[Main] init() complete.")
end

function update(dt)
    frameCount = frameCount + 1; fpsAccum = fpsAccum + dt; fpsFrames = fpsFrames + 1
    if fpsAccum >= 1.0 then fps = fpsFrames / fpsAccum; fpsAccum = 0; fpsFrames = 0 end
    animate.update(dt)
    local scancodes = keys.getAllScancodes()
    for _, sc in ipairs(scancodes) do
        local isDown = isKeyDown(sc); local n = keys.getName(sc)
        if n then
            local prevDown = activeTags["input."..n]; activeTags["input."..n] = isDown or nil
            if isDown and not prevDown then
                events.dispatch(events.createEvent(events.Type.KEY_DOWN, {key=n, scancode=sc, modifiers=getAllActiveTags()}))
                if keys.isPrintable(sc) then
                    local char = isShiftDown() and keys.getShiftedChar(sc) or keys.getChar(sc)
                    if char then events.dispatch(events.createEvent(events.Type.TEXT_INPUT, {text=char, modifiers=getAllActiveTags()})) end
                end
            elseif not isDown and prevDown then events.dispatch(events.createEvent(events.Type.KEY_UP, {key=n, scancode=sc, modifiers=getAllActiveTags()})) end
        end
    end
    activeTags["input.SHIFT"] = isShiftDown() or nil; activeTags["input.CTRL"] = isCtrlDown() or nil; activeTags["input.ALT"] = isAltDown() or nil
    activeTags["input.MouseLEFT"] = isMouseDown(0) or nil; activeTags["input.MouseMIDDLE"] = isMouseDown(1) or nil; activeTags["input.MouseRIGHT"] = isMouseDown(2) or nil
    local mx, my = getMousePos(); local tags = getAllActiveTags(); setbox.setActiveTags(tags); uiState.beginFrame()
    local wheel = getMouseWheel(); if wheel ~= 0 then events.dispatch(events.createEvent(events.Type.MOUSE_WHEEL, {x=mx,y=my,delta=wheel,modifiers=tags})) end
    for b = 0, 2 do
        if isMouseClicked(b) then events.dispatch(events.createEvent(events.Type.MOUSE_DOWN, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=tags}))
        elseif isMouseReleased(b) then events.dispatch(events.createEvent(events.Type.MOUSE_UP, {x=mx,y=my,button=BUTTON_NAMES[b+1],modifiers=tags})) end
    end
    if mx ~= lastMouseX or my ~= lastMouseY then
        local dx, dy = mx - lastMouseX, my - lastMouseY
        events.dispatch(events.createEvent(events.Type.MOUSE_MOVE, {x=mx,y=my,dx=dx,dy=dy,modifiers=tags}))
        lastMouseX, lastMouseY = mx, my
    end
    AppController.sync()
    if Hardware.isHardwareEnabled() then
        if frameCount % 10 == 0 then AppController.pollState() end
        local hwSpec = Hardware.getSpectrum(); if hwSpec and #hwSpec > 0 then spectrumData = hwSpec; Hardware.updateWaterfall(spectrumData) end
    else Hardware.updateWaterfall(spectrumData) end
end

function draw()
    local winW, winH = getWindowSize(); local mx, my = getMousePos(); local tags = getAllActiveTags()
    events.clearWidgets(); ui.beginFrame(); setbox.setActiveTags(tags)
    local regions = container.solve(winW, winH); if not regions then return end

    -- Root panels and their contexts
    local sideContexts = {}
    local rL = regions["left-sidebar"]
    if rL then 
        sideContexts["left-sidebar"] = setbox.newContext({"widget.Sidebar", "widget.LeftSidebar"})
        uiInstances.leftPanel:draw("left-sidebar", rL.x, rL.y, rL.w, rL.h, sideContexts["left-sidebar"]) 
    end
    local rC = regions["center-area"]
    if rC then 
        sideContexts["center-area"] = setbox.newContext({"widget.CenterArea"})
        uiInstances.centerPanel:draw("center-panel", rC.x, rC.y, rC.w, rC.h, sideContexts["center-area"]) 
    end
    local rR = regions["right-sidebar"]
    if rR then 
        sideContexts["right-sidebar"] = setbox.newContext({"widget.Sidebar", "widget.RightSidebar"})
        uiInstances.rightPanel:draw("right-sidebar", rR.x, rR.y, rR.w, rR.h, sideContexts["right-sidebar"]) 
    end
    local rA = regions["active-tags"]
    if rA then 
        sideContexts["active-tags"] = setbox.newContext({"widget.Sidebar", "widget.ActiveTagsSidebar"})
        uiInstances.tagsPanel:draw("active-tags-bg", rA.x, rA.y, rA.w, rA.h, sideContexts["active-tags"]) 
    end

    -- Draw all content widgets
    local currentMode = Model.rx.selectedMode:get()
    for id, r in pairs(regions) do
        local widget = widgets[id]
        if widget then
            local shouldDraw = true
            if id == "id-cw" and currentMode ~= "CW" then shouldDraw = false end
            
            if shouldDraw then
                -- Determine parent context for this widget
                local parentId = "root"
                local allRules = setbox.getRules()
                for _, rule in ipairs(allRules) do
                    if rule.id == id or (rule.tags and rule.tags["id." .. id]) then
                        if rule.properties and rule.properties.parent then
                            parentId = rule.properties.parent
                            break
                        end
                    end
                end
                local pLWC = sideContexts[parentId] or setbox.newContext({})

                if id == "id-smeter" then
                    widget:draw(id, r.x, r.y, r.w, r.h, pLWC, smeter.getReading())
                elseif id == "id-active-tags" then
                    widget:draw(id, r.x, r.y, r.w, r.h, pLWC, tags)
                elseif id == "id-rx-freq" then
                    widget:draw(id, r.x, r.y, r.w, r.h, pLWC, _G.freqEntryText, _G.freqEntryCursor, {"VFOControl"})
                elseif id == "id-rx-slider" then
                    widget:draw(id, r.x, r.y, r.w, r.h, pLWC, 0.1e6, 30.0e6, Model.rx.VFO.activeValue:peek())
                elseif id == "id-rx-vol" then
                    widget:draw(id, r.x, r.y, r.w, r.h, pLWC, -60, 0, Model.rx.volume.DB:peek())
                elseif id == "id-rx-gain" then
                    widget:draw(id, r.x, r.y, r.w, r.h, pLWC, -20, 60, Model.rx.RF.gainDB:peek())
                else
                    widget:draw(id, r.x, r.y, r.w, r.h, pLWC)
                end
            end
        end
    end

    -- Top bar (drawn on top)
    local rT = regions["top-bar"]
    if rT then
        drawRect(rT.x, rT.y, rT.w, rT.h, 0.08, 0.08, 0.12, 1.0)
        local fC = Hardware.isHardwareEnabled() and hw.getFramesReceived and hw.getFramesReceived() or 0
        drawText(rT.x + 10, rT.y + 8, string.format("NexRx | %.0f FPS | Frames: %d", fps, fC), 0.6, 0.7, 0.9, 1.0)
    end

    if Edit.isEditModifierHeld(tags) then Edit.drawHandles(mx, my, events.getWidgetAt(mx, my), true) end
    ui.endFrame(); uiState.endFrame()
end
