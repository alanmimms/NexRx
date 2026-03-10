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
local function projection(name, typeObj)
    local obs = R.computed(function()
        return typeObj:get(name)
    end)
    -- Add set method to allow widgets to mutate the model via this reference
    function obs:set(value)
        Model.set(name, value)
    end
    return obs
end

-- =============================================================================
-- Model Structure (Hierarchical)
-- =============================================================================

Model.rx = {
    active = projection("rx.active", Types.Bool),
    VFO = {
        A = projection("rx.VFO.A", Types.Number),
        B = projection("rx.VFO.B", Types.Number),
        active = projection("rx.VFO.active", Types.String)
    },
    selectedMode = projection("rx.selectedMode", Types.String),
    selectedBand = projection("rx.selectedBand", Types.String),
    volume = {
        DB = projection("rx.volume.DB", Types.Number),
        muted = projection("rx.volume.muted", Types.Bool)
    },
    AGC = {
        enabled = projection("rx.AGC.enabled", Types.Bool),
        mode = projection("rx.AGC.mode", Types.Number)
    },
    RF = {
        gainDB = projection("rx.RF.gainDB", Types.Number),
        attenuationDB = projection("rx.RF.attenuationDB", Types.Number)
    },
    QSD = {
        offsetK = projection("rx.QSD.offsetK", Types.Number)
    },
    DSP = {
        NR = { enabled = projection("rx.NR.enabled", Types.Bool) },
        NB = { enabled = projection("rx.NB.enabled", Types.Bool) },
        bandpass = {
            enabled = projection("rx.bandpass.enabled", Types.Bool),
            width = projection("rx.bandpass.width", Types.Number),
            center = projection("rx.bandpass.center", Types.Number)
        },
        notch = {
            enabled = projection("rx.notch.enabled", Types.Bool),
            width = projection("rx.notch.width", Types.Number),
            center = projection("rx.notch.center", Types.Number)
        }
    },
    CW = {
        pitch = projection("rx.CW.pitch", Types.Number)
    },
    testToneEnabled = projection("rx.testToneEnabled", Types.Bool)
}

Model.preselector = {
    enabled = projection("preselector.enabled", Types.Bool),
    auto = projection("preselector.auto", Types.Bool),
    L = projection("preselector.L", Types.Bool),
    capMask = projection("preselector.capMask", Types.Number)
}

Model.ISG = {
    enabled = projection("isgEnabled", Types.Bool),
    frequencyHz = projection("isgFrequency", Types.Number)
}

Model.waterfall = {
    minDB = projection("wfMinDB", Types.Number),
    maxDB = projection("wfMaxDB", Types.Number),
    colormap = projection("wfColormap", Types.String)
}

-- =============================================================================
-- Mutation Support (Rule Overrides)
-- =============================================================================

local mutationRules = {}

--- Set a model value by creating a high-priority SetBox override rule
function Model.set(name, value)
    -- Create or update a high-priority rule (1000) for this property.
    mutationRules[name] = setbox.rule({
        id = "override." .. name,
        priority = 1000,
        apply = { [name] = value }
    })
end

return Model
