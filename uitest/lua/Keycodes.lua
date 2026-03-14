--[[
    keycodes.lua - Raylib Key Code Definitions and Key Name Mapping

    Provides Raylib key codes as Lua constants and bidirectional mapping
    between key codes and human-readable key names.
]]

local Keys = {}

-- =============================================================================
-- Raylib Key Codes (from raylib.h)
-- =============================================================================

-- Alphanumeric keys
Keys.KEY_APOSTROPHE      = 39
Keys.KEY_COMMA           = 44
Keys.KEY_MINUS           = 45
Keys.KEY_PERIOD          = 46
Keys.KEY_SLASH           = 47
Keys.KEY_ZERO            = 48
Keys.KEY_ONE             = 49
Keys.KEY_TWO             = 50
Keys.KEY_THREE           = 51
Keys.KEY_FOUR            = 52
Keys.KEY_FIVE            = 53
Keys.KEY_SIX             = 54
Keys.KEY_SEVEN           = 55
Keys.KEY_EIGHT           = 56
Keys.KEY_NINE            = 57
Keys.KEY_SEMICOLON       = 59
Keys.KEY_EQUAL           = 61
Keys.KEY_A               = 65
Keys.KEY_B               = 66
Keys.KEY_C               = 67
Keys.KEY_D               = 68
Keys.KEY_E               = 69
Keys.KEY_F               = 70
Keys.KEY_G               = 71
Keys.KEY_H               = 72
Keys.KEY_I               = 73
Keys.KEY_J               = 74
Keys.KEY_K               = 75
Keys.KEY_L               = 76
Keys.KEY_M               = 77
Keys.KEY_N               = 78
Keys.KEY_O               = 79
Keys.KEY_P               = 80
Keys.KEY_Q               = 81
Keys.KEY_R               = 82
Keys.KEY_S               = 83
Keys.KEY_T               = 84
Keys.KEY_U               = 85
Keys.KEY_V               = 86
Keys.KEY_W               = 87
Keys.KEY_X               = 88
Keys.KEY_Y               = 89
Keys.KEY_Z               = 90
Keys.KEY_LEFT_BRACKET    = 91
Keys.KEY_BACKSLASH       = 92
Keys.KEY_RIGHT_BRACKET   = 93
Keys.KEY_GRAVE           = 96

-- Control keys
Keys.KEY_SPACE           = 32
Keys.KEY_ESCAPE          = 256
Keys.KEY_ENTER           = 257
Keys.KEY_TAB             = 258
Keys.KEY_BACKSPACE       = 259
Keys.KEY_INSERT          = 260
Keys.KEY_DELETE          = 261
Keys.KEY_RIGHT           = 262
Keys.KEY_LEFT            = 263
Keys.KEY_DOWN            = 264
Keys.KEY_UP              = 265
Keys.KEY_PAGE_UP         = 266
Keys.KEY_PAGE_DOWN       = 267
Keys.KEY_HOME            = 268
Keys.KEY_END             = 269
Keys.KEY_CAPS_LOCK       = 280
Keys.KEY_SCROLL_LOCK     = 281
Keys.KEY_NUM_LOCK        = 282
Keys.KEY_PRINT_SCREEN    = 283
Keys.KEY_PAUSE           = 284
Keys.KEY_F1              = 290
Keys.KEY_F2              = 291
Keys.KEY_F3              = 292
Keys.KEY_F4              = 293
Keys.KEY_F5              = 294
Keys.KEY_F6              = 295
Keys.KEY_F7              = 296
Keys.KEY_F8              = 297
Keys.KEY_F9              = 298
Keys.KEY_F10             = 299
Keys.KEY_F11             = 300
Keys.KEY_F12             = 301
Keys.KEY_LEFT_SHIFT      = 340
Keys.KEY_LEFT_CONTROL    = 341
Keys.KEY_LEFT_ALT        = 342
Keys.KEY_LEFT_SUPER      = 343
Keys.KEY_RIGHT_SHIFT     = 344
Keys.KEY_RIGHT_CONTROL   = 345
Keys.KEY_RIGHT_ALT       = 346
Keys.KEY_RIGHT_SUPER     = 347
Keys.KEY_KB_MENU         = 348

-- Keypad keys
Keys.KEY_KP_0            = 320
Keys.KEY_KP_1            = 321
Keys.KEY_KP_2            = 322
Keys.KEY_KP_3            = 323
Keys.KEY_KP_4            = 324
Keys.KEY_KP_5            = 325
Keys.KEY_KP_6            = 326
Keys.KEY_KP_7            = 327
Keys.KEY_KP_8            = 328
Keys.KEY_KP_9            = 329
Keys.KEY_KP_DECIMAL      = 330
Keys.KEY_KP_DIVIDE       = 331
Keys.KEY_KP_MULTIPLY     = 332
Keys.KEY_KP_SUBTRACT     = 333
Keys.KEY_KP_ADD          = 334
Keys.KEY_KP_ENTER        = 335
Keys.KEY_KP_EQUAL        = 336

-- =============================================================================
-- Key Code to Name Mapping
-- =============================================================================

local codeToName = {
    [Keys.KEY_A] = "A", [Keys.KEY_B] = "B", [Keys.KEY_C] = "C", [Keys.KEY_D] = "D", [Keys.KEY_E] = "E",
    [Keys.KEY_F] = "F", [Keys.KEY_G] = "G", [Keys.KEY_H] = "H", [Keys.KEY_I] = "I", [Keys.KEY_J] = "J",
    [Keys.KEY_K] = "K", [Keys.KEY_L] = "L", [Keys.KEY_M] = "M", [Keys.KEY_N] = "N", [Keys.KEY_O] = "O",
    [Keys.KEY_P] = "P", [Keys.KEY_Q] = "Q", [Keys.KEY_R] = "R", [Keys.KEY_S] = "S", [Keys.KEY_T] = "T",
    [Keys.KEY_U] = "U", [Keys.KEY_V] = "V", [Keys.KEY_W] = "W", [Keys.KEY_X] = "X", [Keys.KEY_Y] = "Y",
    [Keys.KEY_Z] = "Z",

    [Keys.KEY_ZERO] = "0", [Keys.KEY_ONE] = "1", [Keys.KEY_TWO] = "2", [Keys.KEY_THREE] = "3", [Keys.KEY_FOUR] = "4",
    [Keys.KEY_FIVE] = "5", [Keys.KEY_SIX] = "6", [Keys.KEY_SEVEN] = "7", [Keys.KEY_EIGHT] = "8", [Keys.KEY_NINE] = "9",

    [Keys.KEY_ENTER] = "ENTER", [Keys.KEY_ESCAPE] = "ESC", [Keys.KEY_BACKSPACE] = "BACKSPACE",
    [Keys.KEY_TAB] = "TAB", [Keys.KEY_SPACE] = "SPACE",

    [Keys.KEY_MINUS] = "MINUS", [Keys.KEY_EQUAL] = "EQUALS", [Keys.KEY_LEFT_BRACKET] = "LEFTBRACKET",
    [Keys.KEY_RIGHT_BRACKET] = "RIGHTBRACKET", [Keys.KEY_BACKSLASH] = "BACKSLASH", [Keys.KEY_SEMICOLON] = "SEMICOLON",
    [Keys.KEY_APOSTROPHE] = "APOSTROPHE", [Keys.KEY_GRAVE] = "GRAVE", [Keys.KEY_COMMA] = "COMMA",
    [Keys.KEY_PERIOD] = "PERIOD", [Keys.KEY_SLASH] = "SLASH",

    [Keys.KEY_CAPS_LOCK] = "CAPSLOCK", [Keys.KEY_SCROLL_LOCK] = "SCROLLLOCK", [Keys.KEY_NUM_LOCK] = "NUMLOCK",

    [Keys.KEY_F1] = "F1", [Keys.KEY_F2] = "F2", [Keys.KEY_F3] = "F3", [Keys.KEY_F4] = "F4",
    [Keys.KEY_F5] = "F5", [Keys.KEY_F6] = "F6", [Keys.KEY_F7] = "F7", [Keys.KEY_F8] = "F8",
    [Keys.KEY_F9] = "F9", [Keys.KEY_F10] = "F10", [Keys.KEY_F11] = "F11", [Keys.KEY_F12] = "F12",

    [Keys.KEY_PRINT_SCREEN] = "PRINTSCREEN", [Keys.KEY_PAUSE] = "PAUSE", [Keys.KEY_INSERT] = "INSERT",
    [Keys.KEY_HOME] = "HOME", [Keys.KEY_PAGE_UP] = "PAGEUP", [Keys.KEY_DELETE] = "DELETE",
    [Keys.KEY_END] = "END", [Keys.KEY_PAGE_DOWN] = "PAGEDOWN",
    [Keys.KEY_RIGHT] = "RIGHT", [Keys.KEY_LEFT] = "LEFT", [Keys.KEY_DOWN] = "DOWN", [Keys.KEY_UP] = "UP",

    [Keys.KEY_LEFT_CONTROL] = "LCTRL", [Keys.KEY_LEFT_SHIFT] = "LSHIFT", [Keys.KEY_LEFT_ALT] = "LALT", [Keys.KEY_LEFT_SUPER] = "LGUI",
    [Keys.KEY_RIGHT_CONTROL] = "RCTRL", [Keys.KEY_RIGHT_SHIFT] = "RSHIFT", [Keys.KEY_RIGHT_ALT] = "RALT", [Keys.KEY_RIGHT_SUPER] = "RGUI",
}

-- Build reverse mapping (name -> code)
local nameToCode = {}
for code, name in pairs(codeToName) do
    nameToCode[name] = code
    nameToCode[name:lower()] = code
end

-- =============================================================================
-- Public API
-- =============================================================================

function Keys.getName(code)
    return codeToName[code] or string.format("Unknown(%d)", code)
end

function Keys.getCode(name)
    return nameToCode[name] or nameToCode[name:lower()]
end

function Keys.getAllCodes()
    local codes = {}
    for code, _ in pairs(codeToName) do
        table.insert(codes, code)
    end
    return codes
end

return Keys
