-- NexRx Digital Twin - Default Stimulus Configuration
--
-- This file is loaded when the twin starts to set up simulated
-- antenna signals for testing the receiver.
--
-- Available functions:
--   stimulus.addMorse(name, {freq, amplitude, text, wpm, repeat})
--   stimulus.addSsb(name, {freq, amplitude, mode, tones/voice, repeat})
--   stimulus.addTone(name, {freq, amplitude})
--   stimulus.addNoise(name, {rms, type})
--   stimulus.remove(name)
--   stimulus.clear()
--   stimulus.list()

-- Signal level reference (50Ω):
-- S9+40 = 5mV      S9+20 = 500µV    S9 = 50µV
-- S7 = 12.5µV      S5 = 3µV         S1 = 0.2µV

print("[Stimulus] Loading default configuration (Boosted Levels)...")

-- Background noise
stimulus.addNoise("band-noise", {
    rms = 1e-6,
    type = "thermal"
})

if true then

-- CW beacon on 14.100 MHz (S7 level)
stimulus.addMorse("VVV-DE-NIST", {
    freq = 14.0750e6,
    amplitude = 1.25e-3, -- 1.25mV
    text = "VVV DE NIST FORT COLLINS COLORADO",
    wpm = 18,
    loop = true
})

-- CW station calling CQ on 14.025 MHz (S9 level)
stimulus.addMorse("cq-WA1AW", {
    freq = 14.025e6,
    amplitude = 5.0e-3, -- 5mV
    text = "CQ CQ CQ DE W1AW W1AW K",
    wpm = 22,
    loop = true
})

-- SSB two-tone test signal on 14.120 MHz (S9+10)
stimulus.addSsb("ssb-2tone", {
    freq = 14.120e6,
    amplitude = 15e-3, -- 15mV
    mode = "usb",
    tones = {700, 1900}  -- Standard two-tone IMD test
})

-- SSB single tone on 14.150 MHz (S5)
stimulus.addSsb("ssb-1tone", {
    freq = 14.150e6,
    amplitude = 300e-6, -- 0.3mV
    mode = "usb",
    tones = {1000}
})

-- AM two-tone beacon on 14.250 MHz (S9+10)
stimulus.addAm("am-2tone", {
    freq = 14.250e6,
    amplitude = 15e-3, -- 15mV
    modIndex = 0.8,
    tones = {400, 1000}
})

-- AM voice beacon on 14.280 MHz (S9+10)
stimulus.addAm("wwv15", {
    freq = 15e6,
    amplitude = 15e-3, -- 15mV
    modIndex = 0.9,
    audioFile = "test/wwv-ident.wav",
    loop = true
})

end

stimulus.addSsb("voice-id", {
    freq = 14.200e6,
    amplitude = 15e-3, -- 15mV
    mode = "usb",
    audioFile = "test/CQ-WB7NAB-gb-fem-8k.wav",
    loop = true
})



print("[Stimulus] Loaded " .. stimulus.count() .. " stimuli")

-- List what we loaded
for name, info in pairs(stimulus.list()) do
    local freqMhz = info.freq / 1e6
    local ampUv = info.amplitude * 1e6
    print(string.format("  %s: %s @ %.3f MHz, %.1f µV",
        name, info.type, freqMhz, ampUv))
end
