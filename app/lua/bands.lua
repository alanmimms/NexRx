--[[
    bands.lua - Band Management Module

    Provides reactive band detection based on frequency.
    Band definitions come from SetBox rules (config/bands.lua).
    When frequency changes, the band tag is automatically updated,
    triggering re-evaluation of band-specific SetBox rules.

    Usage:
        local bands = require("bands")
        bands.init()
        bands.setFrequency(14.2e6)  -- Automatically updates "20m" tag
        local band = bands.getCurrent()  -- Returns "20m"
]]

local Bands = {}

-- Band definitions loaded from SetBox
-- Each entry: {name, startHz, endHz, defaultFreqHz}
Bands.definitions = {}

-- Current state
Bands.current = nil      -- Current band name (or "OOB")
Bands.frequencyHz = 0    -- Current frequency in Hz

-- Standard amateur bands (loaded via init() from SetBox)
-- These serve as defaults if config/bands.lua isn't loaded
local defaultBands = {
    {name = "160m", startHz = 1.8e6,   endHz = 2.0e6,   defaultFreqHz = 1.9e6},
    {name = "80m",  startHz = 3.5e6,   endHz = 4.0e6,   defaultFreqHz = 3.75e6},
    {name = "60m",  startHz = 5.3305e6, endHz = 5.4065e6, defaultFreqHz = 5.3665e6},
    {name = "40m",  startHz = 7.0e6,   endHz = 7.3e6,   defaultFreqHz = 7.15e6},
    {name = "30m",  startHz = 10.1e6,  endHz = 10.15e6, defaultFreqHz = 10.125e6},
    {name = "20m",  startHz = 14.0e6,  endHz = 14.35e6, defaultFreqHz = 14.2e6},
    {name = "17m",  startHz = 18.068e6, endHz = 18.168e6, defaultFreqHz = 18.1e6},
    {name = "15m",  startHz = 21.0e6,  endHz = 21.45e6, defaultFreqHz = 21.2e6},
    {name = "12m",  startHz = 24.89e6, endHz = 24.99e6, defaultFreqHz = 24.93e6},
    {name = "10m",  startHz = 28.0e6,  endHz = 29.7e6,  defaultFreqHz = 28.5e6},
    {name = "6m",   startHz = 50.0e6,  endHz = 54.0e6,  defaultFreqHz = 50.1e6},
}

--- Initialize the band system
-- Loads band definitions from SetBox and sets up callbacks
function Bands.init()
    -- Load band definitions from SetBox rules with "BandDef" tag
    Bands._loadFromSetBox()

    -- If no bands loaded from SetBox, use defaults
    if #Bands.definitions == 0 then
        Bands.definitions = defaultBands
        print("[Bands] Using built-in band definitions")
    else
        print("[Bands] Loaded " .. #Bands.definitions .. " bands from SetBox")
    end
end

--- Load band definitions from SetBox
-- Scans for rules with "BandDef" tag
function Bands._loadFromSetBox()
    if not setbox or not setbox.getRulesWithTag then
        -- SetBox doesn't have getRulesWithTag yet, use defaults
        return
    end

    local bandRules = setbox.getRulesWithTag("BandDef")
    if not bandRules then return end

    Bands.definitions = {}
    for _, rule in ipairs(bandRules) do
        local props = rule.apply or rule
        if props.bandName and props.bandStartHz and props.bandEndHz then
            table.insert(Bands.definitions, {
                name = props.bandName,
                startHz = props.bandStartHz,
                endHz = props.bandEndHz,
                defaultFreqHz = props.bandDefaultFreq or props.bandStartHz,
            })
        end
    end

    -- Sort by start frequency
    table.sort(Bands.definitions, function(a, b)
        return a.startHz < b.startHz
    end)
end

--- Set the current frequency and update band tag if changed
-- @param freqHz frequency in Hz
function Bands.setFrequency(freqHz)
    Bands.frequencyHz = freqHz
    local newBand = Bands._getBandForFreq(freqHz)

    if newBand ~= Bands.current then
        -- Band changed - update SetBox tags
        Bands._updateBandTag(Bands.current, newBand)
        Bands.current = newBand
    end
end

--- Get current band name
-- @return band name string (e.g., "20m") or "OOB" if out of band
function Bands.getCurrent()
    return Bands.current or "OOB"
end

--- Get band info for current frequency
-- @return band table {name, startHz, endHz, defaultFreqHz} or nil
function Bands.getCurrentInfo()
    return Bands.getBandInfo(Bands.current)
end

--- Get band info by name
-- @param bandName band name (e.g., "20m")
-- @return band table or nil
function Bands.getBandInfo(bandName)
    if not bandName then return nil end
    for _, band in ipairs(Bands.definitions) do
        if band.name == bandName then
            return band
        end
    end
    return nil
end

--- Check if a frequency is within a specific band
-- @param freqHz frequency in Hz
-- @param bandName band name to check
-- @return boolean
function Bands.isInBand(freqHz, bandName)
    local band = Bands.getBandInfo(bandName)
    if not band then return false end
    return freqHz >= band.startHz and freqHz <= band.endHz
end

--- Get all band names in frequency order
-- @return array of band names
function Bands.getAllNames()
    local names = {}
    for _, band in ipairs(Bands.definitions) do
        table.insert(names, band.name)
    end
    return names
end

--- Internal: find band for a frequency
-- @param freqHz frequency in Hz
-- @return band name or "OOB"
function Bands._getBandForFreq(freqHz)
    for _, band in ipairs(Bands.definitions) do
        if freqHz >= band.startHz and freqHz <= band.endHz then
            return band.name
        end
    end
    return "OOB"
end

--- Internal: update SetBox tags when band changes
-- @param oldBand previous band name (or nil)
-- @param newBand new band name
function Bands._updateBandTag(oldBand, newBand)
    if not setbox then return end

    -- Remove old band tag
    if oldBand and oldBand ~= "OOB" and setbox.removeTag then
        setbox.removeTag(oldBand)
    end

    -- Add new band tag
    if newBand and newBand ~= "OOB" and setbox.addTag then
        setbox.addTag(newBand)
    end

    -- Log the change
    if oldBand ~= newBand then
        print(string.format("[Bands] Band changed: %s -> %s",
            oldBand or "none", newBand))
    end
end

--- Get default frequency for a band
-- @param bandName band name
-- @return frequency in Hz, or nil if band not found
function Bands.getDefaultFreq(bandName)
    local band = Bands.getBandInfo(bandName)
    if band then
        return band.defaultFreqHz
    end
    return nil
end

return Bands
