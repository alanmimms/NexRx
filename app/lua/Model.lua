--[[
    Model.lua - Structured Reactive Model for NexRx
    
    Acts as a live projection of SetBox rules. Each property is a
    Reactive.computed that automatically tracks its tag dependencies.
    Uses polymorphic Type Objects for resolution.
]]

local R = require("Reactive")
local setbox = require("SetBox")

local Model = {}

-- =============================================================================
-- Type Objects (Polymorphic Interface)
-- =============================================================================

local Types = {
    Number = { get = function(_, name) return setbox.getNumber(name) end },
    Bool   = { get = function(_, name) return setbox.getBool(name) end },
    String = { get = function(_, name) return setbox.getString(name) end }
}

-- Polymorphic projection factory
local function projection(name, typeObj, default)
    local obs = R.computed(function()
        local ok, val = pcall(function() return typeObj:get(name) end)
        if ok and val ~= nil then return val end
        return default
    end)
    -- Add set method to allow widgets to mutate the model via this reference
    function obs:set(value)
        Model.set(name, value)
    end
    return obs
end

-- =============================================================================
-- SignalBox Management
-- =============================================================================

-- List of SignalBox observables. Each is an observable object {frequency, mode, bandwidth, id, ghost}
Model.signalBoxes = R.observable({})
Model.selectedSignalBoxIndex = R.observable(1)

function Model.addSignalBox(freq, mode, bandwidth, ghost)
    local boxes = Model.signalBoxes:peek()
    local newId = 1
    for _, b in ipairs(boxes) do
        if b.id >= newId then newId = b.id + 1 end
    end
    
    local newBox = R.observableObject({
        frequency = freq or 14.2e6,
        mode = mode or "USB",
        bandwidth = bandwidth or 4000,
        id = newId,
        ghost = ghost or false
    })
    
    local newList = {}
    for _, b in ipairs(boxes) do table.insert(newList, b) end
    table.insert(newList, newBox)
    Model.signalBoxes:set(newList)
    return #newList
end

function Model.removeSignalBox(index)
    local boxes = Model.signalBoxes:peek()
    if #boxes <= 1 then return end -- Always keep at least one
    
    local newList = {}
    for i, b in ipairs(boxes) do
        if i ~= index then table.insert(newList, b) end
    end
    Model.signalBoxes:set(newList)
    
    local currentIdx = Model.selectedSignalBoxIndex:peek()
    if currentIdx > #newList then
        Model.selectedSignalBoxIndex:set(#newList)
    elseif currentIdx == index then
        -- Keep index the same (points to next item or end)
    end
end

function Model.getSelectedSignalBox()
    local boxes = Model.signalBoxes:get()
    local idx = Model.selectedSignalBoxIndex:get()
    return boxes[idx]
end

-- =============================================================================
-- Model Structure (Hierarchical)
-- =============================================================================

Model.rx = {
    active = projection("rx.active", Types.Bool, true),
    
    -- VFO properties now proxy the selected SignalBox
    VFO = {
        active = R.computed(function() 
            local box = Model.getSelectedSignalBox()
            return box and tostring(box.id) or "1"
        end),
        activeValue = R.computed(function()
            local box = Model.getSelectedSignalBox()
            return box and box.frequency or 14.2e6
        end)
    },
    
    -- Mode now proxies the selected SignalBox
    selectedMode = R.computed(function()
        local box = Model.getSelectedSignalBox()
        return box and box.mode or "USB"
    end),
    
    selectedBand = projection("rx.selectedBand", Types.String, "40m"),
    volume = {
        DB = projection("rx.volume.DB", Types.Number, -20),
        muted = projection("rx.volume.muted", Types.Bool, false)
    },
    AGC = {
        enabled = projection("rx.AGC.enabled", Types.Bool, true),
        mode = projection("rx.AGC.mode", Types.Number, 0)
    },
    RF = {
        gainDB = projection("rx.RF.gainDB", Types.Number, 0),
        attenuationDB = projection("rx.RF.attenuationDB", Types.Number, 0)
    },
    QSD = {
        offsetK = projection("rx.QSD.offsetK", Types.Number, 0)
    },
    DSP = {
        NR = { enabled = projection("rx.NR.enabled", Types.Bool, false) },
        NB = { enabled = projection("rx.NB.enabled", Types.Bool, false) },
        bandpass = {
            enabled = projection("rx.bandpass.enabled", Types.Bool, true),
            width = projection("rx.bandpass.width", Types.Number, 3000),
            center = projection("rx.bandpass.center", Types.Number, 0)
        },
        notch = {
            enabled = projection("rx.notch.enabled", Types.Bool, false),
            width = projection("rx.notch.width", Types.Number, 200),
            center = projection("rx.notch.center", Types.Number, 1000)
        }
    },
    CW = {
        pitch = projection("rx.CW.pitch", Types.Number, 600)
    },
    testToneEnabled = projection("rx.testToneEnabled", Types.Bool, false)
}

-- Spectrum center is an observable we can shift
Model.spectrumCenterFreq = R.observable(14.2e6)

Model.preselector = {
    enabled = projection("preselector.enabled", Types.Bool, true),
    auto = projection("preselector.auto", Types.Bool, true),
    L = projection("preselector.L", Types.Bool, true),
    capMask = projection("preselector.capMask", Types.Number, 0)
}

Model.ISG = {
    enabled = projection("isgEnabled", Types.Bool, false),
    frequencyHz = projection("isgFrequency", Types.Number, 14.0e6)
}

Model.waterfall = {
    minDB = projection("wfMinDB", Types.Number, -120),
    maxDB = projection("wfMaxDB", Types.Number, -40),
    colormap = projection("wfColormap", Types.String, "viridis"),
    zoom = projection("spectrumZoom", Types.Number, 1.0)
}

-- =============================================================================
-- Mutation Support (Rule Overrides)
-- =============================================================================

local mutationRules = {}

--- Set a model value by creating a high-priority SetBox override rule
function Model.set(name, value)
    -- Handle special proxy cases for backward compatibility
    if name == "rx.selectedMode" then
        local box = Model.getSelectedSignalBox()
        if box then box.mode = value end
        return
    elseif name:match("^rx%.VFO%.") then
        local box = Model.getSelectedSignalBox()
        if box then box.frequency = value end
        return
    end

    -- Create or update a high-priority rule (1000) for this property.
    mutationRules[name] = setbox.rule({
        id = "override." .. name,
        priority = 1000,
        apply = { [name] = value }
    })
end

--- Round frequency to nearest step
function Model.roundFrequency(name, stepHz)
    local current = 0
    if name:match("^rx%.VFO%.") then
        local box = Model.getSelectedSignalBox()
        current = box and box.frequency or 0
    else
        current = setbox.getNumber(name)
    end
    local rounded = math.floor(current / stepHz + 0.5) * stepHz
    Model.set(name, rounded)
end

-- Initialize with one box
Model.addSignalBox(14.2e6, "USB", 4000, false)

return Model
