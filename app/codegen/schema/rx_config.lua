-- NexRx Receiver Configuration Schema
--
-- This file defines all configuration properties for the receiver.
-- It is processed at build time by generate_config.lua to create C++ code.
--
-- Copyright 2026 NexRx Project - MIT License

return {
    name = "RxConfig",
    namespace = "nexrx",

    properties = {
        -- =================================================================
        -- Audio Properties
        -- =================================================================

        volume = {
            type = "number",
            default = 0.0316,  -- -30 dB (mid-range)
            range = {0.0, 1.0},
            description = "Audio volume (linear 0-1)",
            ui = {
                widget = "slider",
                label = "Volume",
                suffix = "",
                group = "audio"
            }
        },

        squelch = {
            type = "number",
            default = 0.3,
            range = {0.0, 1.0},
            description = "Squelch threshold",
            ui = {
                widget = "slider",
                label = "Squelch",
                suffix = "",
                group = "audio"
            }
        },

        muted = {
            type = "boolean",
            default = false,
            description = "Audio mute state",
            ui = {
                widget = "checkbox",
                label = "Mute",
                group = "audio"
            }
        },

        testToneEnabled = {
            type = "boolean",
            default = false,
            description = "Enable test tone output",
            ui = {
                widget = "checkbox",
                label = "Test Tone",
                group = "audio"
            }
        },

        testToneFreq = {
            type = "number",
            default = 440,
            range = {100, 3000, 1},
            description = "Test tone frequency in Hz",
            ui = {
                widget = "slider",
                label = "Tone Freq",
                suffix = "Hz",
                group = "audio"
            }
        },

        -- =================================================================
        -- Radio/Demodulator Properties
        -- =================================================================

        mode = {
            type = "choice",
            choices = {"USB", "LSB", "CW", "AM"},
            default = "USB",
            description = "Demodulation mode",
            ui = {
                widget = "dropdown",
                label = "Mode",
                group = "radio"
            }
        },

        filterWidth = {
            type = "number",
            default = 2400,
            range = {100, 6000, 50},
            description = "Filter bandwidth in Hz",
            ui = {
                widget = "slider",
                label = "Filter Width",
                suffix = "Hz",
                group = "radio"
            }
        },

        filterShift = {
            type = "number",
            default = 0,
            range = {-2000, 2000, 10},
            description = "Filter passband shift in Hz",
            ui = {
                widget = "slider",
                label = "Filter Shift",
                suffix = "Hz",
                group = "radio"
            }
        },

        agcEnabled = {
            type = "boolean",
            default = true,
            description = "Automatic Gain Control enabled",
            ui = {
                widget = "checkbox",
                label = "AGC",
                group = "dsp"
            }
        },

        nrEnabled = {
            type = "boolean",
            default = false,
            description = "Noise Reduction enabled",
            ui = {
                widget = "checkbox",
                label = "Noise Reduction",
                group = "dsp"
            }
        },

        nbEnabled = {
            type = "boolean",
            default = false,
            description = "Noise Blanker enabled",
            ui = {
                widget = "checkbox",
                label = "Noise Blanker",
                group = "dsp"
            }
        },

        -- =================================================================
        -- Baseband Filter Properties
        -- =================================================================

        bandpassEnabled = {
            type = "boolean",
            default = false,
            description = "Enable baseband bandpass filter",
            ui = {
                widget = "checkbox",
                label = "Bandpass",
                group = "filter"
            }
        },

        bandpassCenter = {
            type = "number",
            default = 700,
            range = {-20000, 20000, 10},
            description = "Bandpass center frequency offset (Hz)",
            ui = {
                widget = "slider",
                label = "BP Center",
                suffix = "Hz",
                group = "filter"
            }
        },

        bandpassWidth = {
            type = "number",
            default = 500,
            range = {50, 6000, 10},
            description = "Bandpass filter bandwidth (Hz)",
            ui = {
                widget = "slider",
                label = "BP Width",
                suffix = "Hz",
                group = "filter"
            }
        },

        bandpassTaps = {
            type = "number",
            default = 127,
            range = {15, 511, 2},
            description = "Bandpass FIR filter taps (steepness)",
            ui = {
                widget = "slider",
                label = "BP Taps",
                suffix = "",
                group = "filter"
            }
        },

        notchEnabled = {
            type = "boolean",
            default = false,
            description = "Enable baseband notch filter",
            ui = {
                widget = "checkbox",
                label = "Notch",
                group = "filter"
            }
        },

        notchCenter = {
            type = "number",
            default = 0,
            range = {-20000, 20000, 10},
            description = "Notch center frequency offset (Hz)",
            ui = {
                widget = "slider",
                label = "Notch Center",
                suffix = "Hz",
                group = "filter"
            }
        },

        notchWidth = {
            type = "number",
            default = 100,
            range = {10, 500, 5},
            description = "Notch filter bandwidth (Hz)",
            ui = {
                widget = "slider",
                label = "Notch Width",
                suffix = "Hz",
                group = "filter"
            }
        },

        -- =================================================================
        -- Twin/Hardware Properties (Remote)
        -- =================================================================

        qsdOffsetK = {
            type = "number",
            default = 12.0,
            range = {1, 24, 0.5},
            description = "QSD offset k for image rejection (kHz)",
            remote = {
                name = "QSD_OFFSET"
            },
            ui = {
                widget = "slider",
                label = "QSD k",
                suffix = "kHz",
                group = "hardware"
            }
        },

        rfAttenuation = {
            type = "number",
            default = 0,
            range = {0, 45, 3},
            description = "RF attenuator setting (dB)",
            remote = {
                name = "ATTENUATION"
            },
            ui = {
                widget = "slider",
                label = "Attenuation",
                suffix = "dB",
                group = "hardware"
            }
        },

        -- =================================================================
        -- Twin Connection Properties
        -- =================================================================

        twinHost = {
            type = "string",
            default = "127.0.0.1",
            description = "Digital twin host address",
            ui = {
                widget = "entry",
                label = "Twin Host",
                group = "connection"
            }
        },

        twinControlPort = {
            type = "number",
            default = 5000,
            range = {1024, 65535, 1},
            description = "Twin TCP control port",
            ui = {
                widget = "entry",
                label = "Control Port",
                group = "connection"
            }
        },

        twinStreamPort = {
            type = "number",
            default = 5001,
            range = {1024, 65535, 1},
            description = "Twin UDP stream port",
            ui = {
                widget = "entry",
                label = "Stream Port",
                group = "connection"
            }
        },

        twinAutoConnect = {
            type = "boolean",
            default = true,
            description = "Auto-connect to twin on startup",
            ui = {
                widget = "checkbox",
                label = "Auto Connect",
                group = "connection"
            }
        },

        -- =================================================================
        -- Display Properties
        -- =================================================================

        colormap = {
            type = "choice",
            choices = {"viridis", "plasma", "inferno", "green", "blue"},
            default = "viridis",
            description = "Waterfall colormap",
            ui = {
                widget = "dropdown",
                label = "Colormap",
                group = "display"
            }
        },

        -- =================================================================
        -- Actions
        -- =================================================================

        resetDefaults = {
            type = "action",
            description = "Reset all settings to defaults",
            ui = {
                widget = "button",
                label = "Reset",
                group = "general"
            }
        },

        connectTwin = {
            type = "action",
            description = "Connect to digital twin",
            ui = {
                widget = "button",
                label = "Connect",
                group = "connection"
            }
        },

        disconnectTwin = {
            type = "action",
            description = "Disconnect from digital twin",
            ui = {
                widget = "button",
                label = "Disconnect",
                group = "connection"
            }
        },
    },

    -- Property groups for UI organization
    groups = {
        audio = {"volume", "squelch", "muted", "testToneEnabled", "testToneFreq"},
        radio = {"mode", "filterWidth", "filterShift"},
        dsp = {"agcEnabled", "nrEnabled", "nbEnabled"},
        filter = {"bandpassEnabled", "bandpassCenter", "bandpassWidth", "bandpassTaps",
                  "notchEnabled", "notchCenter", "notchWidth"},
        hardware = {"qsdOffsetK", "rfAttenuation"},
        connection = {"twinHost", "twinControlPort", "twinStreamPort", "twinAutoConnect", "connectTwin", "disconnectTwin"},
        display = {"colormap"},
        general = {"resetDefaults"},
    }
}
