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

print("[Stimulus] Loading default configuration...")

-- Background noise (about S3 level)
stimulus.addNoise("band_noise", {
    rms = 1e-6,      -- 1µV RMS
    type = "thermal"
})

-- CW beacon on 14.100 MHz (S7 level)
stimulus.addMorse("nist_beacon", {
    freq = 14.0750e6,
    amplitude = 12e-6,  -- ~S7
    text = "VVV DE NIST FORT COLLINS COLORADO",
    wpm = 18,
    ["repeat"] = true
})

-- CW station calling CQ on 14.025 MHz (S9 level)
stimulus.addMorse("cq_station", {
    freq = 14.025e6,
    amplitude = 50e-6,  -- S9
    text = "CQ CQ CQ DE W1AW W1AW K",
    wpm = 22,
    ["repeat"] = true
})

-- SSB two-tone test signal on 14.120 MHz (S9+10)
stimulus.addSsb("two_tone", {
    freq = 14.120e6,
    amplitude = 150e-6,  -- S9+10
    mode = "usb",
    tones = {700, 1900}  -- Standard two-tone IMD test
})

-- SSB single tone on 14.150 MHz (S5)
stimulus.addSsb("ssb_tone", {
    freq = 14.150e6,
    amplitude = 3e-6,    -- S5
    mode = "usb",
    tones = {1000}
})

  stimulus.addSsb("voice-id", {
      freq = 14.200e6,
      mode = "usb",
      audioFile = "test/CQ-WB7NAB-gb-fem-8k.wav",
      loop = true  -- optional, default true
  })

-- stimulus.addSsb("voice_id", {
--     freq = 14.200e6,
--     amplitude = 30e-3,
--     mode = "usb",
--     voice = "CQ CQ CQ this is November Echo X-ray Romeo X-ray calling CQ and standing by.",
--     ["repeat"] = true,
--     -- espeak-ng voice parameters:
--     voiceName = "en-gb",     -- Voice: en, en-us, en-gb, de, es, fr, etc.
--     rate = 150,              -- Words per minute (80-450, default 175)
--     pitch = 40,              -- Pitch (0-100, default 50)
--     -- pitchRange = 50,      -- Pitch range (0-100, default 50)
--     -- volume = 100,         -- Volume (0-200, default 100)
--     wordGap = 3,		-- Gap between words in 10ms units
--     -- capitals = 0,         -- 0=none, 1=sound icon, 2=pitch, 3=both
-- })

print("[Stimulus] Loaded " .. stimulus.count() .. " stimuli")

-- List what we loaded
for name, info in pairs(stimulus.list()) do
    local freqMhz = info.freq / 1e6
    local ampUv = info.amplitude * 1e6
    print(string.format("  %s: %s @ %.3f MHz, %.1f µV",
        name, info.type, freqMhz, ampUv))
end
