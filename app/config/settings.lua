--[[
  NexRx User Settings

  This file contains your personal configuration overrides.
  Any settings here override the defaults from default.lua.

  To customize a setting, uncomment and modify the relevant line below.
  Settings use SetBox rules with priority 50 (overrides defaults at -100).

  Example: To change your default VFO frequencies:

    rule {
        priority = 50,
        apply = {
            vfoA = 7.074e6,     -- FT8 on 40m
            vfoB = 14.074e6,    -- FT8 on 20m
        }
    }

  You can also create conditional rules based on tags:

    rule {
        tags = {"CW"},
        priority = 50,
        apply = {
            wfMinDB = -130,     -- Lower floor for CW
        }
    }

  Available settings (see config/default.lua for complete list):

  Radio:
    defaultFrequency    -- Startup frequency in Hz (e.g., 14.200e6)
    defaultMode         -- Startup mode: "USB", "LSB", "CW", "AM"
    defaultBand         -- Startup band: "160m", "80m", "40m", "20m", "15m", "10m"
    vfoA, vfoB          -- VFO frequencies in Hz
    activeVFO           -- Active VFO: "A" or "B"

  DSP:
    squelch             -- Squelch level 0.0-1.0
    agcEnabled          -- AGC on/off
    nrEnabled           -- Noise reduction on/off
    nbEnabled           -- Noise blanker on/off
    lmsMu               -- LMS learning rate (0.0001-0.1)
    bfoOffset           -- BFO/sidetone frequency in Hz

  Hardware:
    qsdOffsetK          -- QSD offset in kHz
    rfAttenDb           -- RF attenuator in dB

  Display:
    waterfallBins       -- FFT bins (256, 512, 1024, 2048)
    waterfallRows       -- Waterfall history rows
    colormap            -- Colormap: "viridis", "plasma", "inferno", "green", "blue"
    wfMinDB             -- Waterfall floor in dB
    wfMaxDB             -- Waterfall ceiling in dB
    spectrumEmaAlpha    -- Spectrum smoothing (0.0-1.0)

  Window:
    windowWidth         -- Window width in pixels
    windowHeight        -- Window height in pixels
    fontSize            -- UI font size

  Connection:
    hwHost              -- Hardware/twin server address
    hwControlPort       -- TCP control port
    hwStreamPort        -- UDP stream port
    hwAutoConnect       -- Auto-connect on startup

  Audio:
    rxVolume            -- Volume 0.0-1.0
    muted               -- Mute on/off

  Recording:
    recordingPath       -- Path for audio recordings
]]

-- =============================================================================
-- Your settings below this line
-- =============================================================================

rule {
    id = "user-settings",
    priority = 50,
    apply = {
        -- Radio settings
        vfoA = 14.200e6,
        vfoB = 7.050e6,
        defaultFrequency = 14.200e6,
        defaultMode = "USB",

        -- Display settings
        -- colormap = "plasma",
        -- wfMinDB = -110,
        -- wfMaxDB = -30,

        -- Window settings
        -- windowWidth = 1440,
        -- windowHeight = 900,

        -- Audio settings
        -- rxVolume = 0.7,
    }
}

print("[settings.lua] User settings loaded")
