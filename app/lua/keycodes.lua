--[[
    keycodes.lua - SDL Scan Code Definitions and Key Name Mapping

    Provides SDL scan codes as Lua constants and bidirectional mapping
    between scan codes and human-readable key names.

    Usage:
        local keys = require("keycodes")

        if event.scancode == keys.SC_ESCAPE then ... end
        local name = keys.getName(event.scancode)  -- "Escape"
        local code = keys.getCode("Enter")  -- 40
]]

local Keys = {}

-- =============================================================================
-- SDL Scan Codes (from SDL_scancode.h)
-- =============================================================================

-- Letters (A-Z)
Keys.SC_A = 4
Keys.SC_B = 5
Keys.SC_C = 6
Keys.SC_D = 7
Keys.SC_E = 8
Keys.SC_F = 9
Keys.SC_G = 10
Keys.SC_H = 11
Keys.SC_I = 12
Keys.SC_J = 13
Keys.SC_K = 14
Keys.SC_L = 15
Keys.SC_M = 16
Keys.SC_N = 17
Keys.SC_O = 18
Keys.SC_P = 19
Keys.SC_Q = 20
Keys.SC_R = 21
Keys.SC_S = 22
Keys.SC_T = 23
Keys.SC_U = 24
Keys.SC_V = 25
Keys.SC_W = 26
Keys.SC_X = 27
Keys.SC_Y = 28
Keys.SC_Z = 29

-- Numbers (top row)
Keys.SC_1 = 30
Keys.SC_2 = 31
Keys.SC_3 = 32
Keys.SC_4 = 33
Keys.SC_5 = 34
Keys.SC_6 = 35
Keys.SC_7 = 36
Keys.SC_8 = 37
Keys.SC_9 = 38
Keys.SC_0 = 39

-- Special keys
Keys.SC_RETURN = 40
Keys.SC_ENTER = 40  -- Alias
Keys.SC_ESCAPE = 41
Keys.SC_BACKSPACE = 42
Keys.SC_TAB = 43
Keys.SC_SPACE = 44

-- Punctuation
Keys.SC_MINUS = 45
Keys.SC_EQUALS = 46
Keys.SC_LEFTBRACKET = 47
Keys.SC_RIGHTBRACKET = 48
Keys.SC_BACKSLASH = 49
Keys.SC_SEMICOLON = 51
Keys.SC_APOSTROPHE = 52
Keys.SC_GRAVE = 53
Keys.SC_COMMA = 54
Keys.SC_PERIOD = 55
Keys.SC_SLASH = 56

-- Lock keys
Keys.SC_CAPSLOCK = 57
Keys.SC_SCROLLLOCK = 71
Keys.SC_NUMLOCKCLEAR = 83

-- Function keys
Keys.SC_F1 = 58
Keys.SC_F2 = 59
Keys.SC_F3 = 60
Keys.SC_F4 = 61
Keys.SC_F5 = 62
Keys.SC_F6 = 63
Keys.SC_F7 = 64
Keys.SC_F8 = 65
Keys.SC_F9 = 66
Keys.SC_F10 = 67
Keys.SC_F11 = 68
Keys.SC_F12 = 69

-- Navigation
Keys.SC_PRINTSCREEN = 70
Keys.SC_PAUSE = 72
Keys.SC_INSERT = 73
Keys.SC_HOME = 74
Keys.SC_PAGEUP = 75
Keys.SC_DELETE = 76
Keys.SC_END = 77
Keys.SC_PAGEDOWN = 78
Keys.SC_RIGHT = 79
Keys.SC_LEFT = 80
Keys.SC_DOWN = 81
Keys.SC_UP = 82

-- Numpad
Keys.SC_KP_DIVIDE = 84
Keys.SC_KP_MULTIPLY = 85
Keys.SC_KP_MINUS = 86
Keys.SC_KP_PLUS = 87
Keys.SC_KP_ENTER = 88
Keys.SC_KP_1 = 89
Keys.SC_KP_2 = 90
Keys.SC_KP_3 = 91
Keys.SC_KP_4 = 92
Keys.SC_KP_5 = 93
Keys.SC_KP_6 = 94
Keys.SC_KP_7 = 95
Keys.SC_KP_8 = 96
Keys.SC_KP_9 = 97
Keys.SC_KP_0 = 98
Keys.SC_KP_PERIOD = 99

-- Modifiers
Keys.SC_LCTRL = 224
Keys.SC_LSHIFT = 225
Keys.SC_LALT = 226
Keys.SC_LGUI = 227  -- Windows/Command key
Keys.SC_RCTRL = 228
Keys.SC_RSHIFT = 229
Keys.SC_RALT = 230
Keys.SC_RGUI = 231

-- =============================================================================
-- Scan Code to Name Mapping
-- =============================================================================

local codeToName = {
    -- Letters
    [4] = "A", [5] = "B", [6] = "C", [7] = "D", [8] = "E",
    [9] = "F", [10] = "G", [11] = "H", [12] = "I", [13] = "J",
    [14] = "K", [15] = "L", [16] = "M", [17] = "N", [18] = "O",
    [19] = "P", [20] = "Q", [21] = "R", [22] = "S", [23] = "T",
    [24] = "U", [25] = "V", [26] = "W", [27] = "X", [28] = "Y",
    [29] = "Z",

    -- Numbers
    [30] = "1", [31] = "2", [32] = "3", [33] = "4", [34] = "5",
    [35] = "6", [36] = "7", [37] = "8", [38] = "9", [39] = "0",

    -- Special
    [40] = "Enter", [41] = "Escape", [42] = "Backspace",
    [43] = "Tab", [44] = "Space",

    -- Punctuation
    [45] = "Minus", [46] = "Equals", [47] = "LeftBracket",
    [48] = "RightBracket", [49] = "Backslash", [51] = "Semicolon",
    [52] = "Apostrophe", [53] = "Grave", [54] = "Comma",
    [55] = "Period", [56] = "Slash",

    -- Lock keys
    [57] = "CapsLock", [71] = "ScrollLock", [83] = "NumLock",

    -- Function keys
    [58] = "F1", [59] = "F2", [60] = "F3", [61] = "F4",
    [62] = "F5", [63] = "F6", [64] = "F7", [65] = "F8",
    [66] = "F9", [67] = "F10", [68] = "F11", [69] = "F12",

    -- Navigation
    [70] = "PrintScreen", [72] = "Pause", [73] = "Insert",
    [74] = "Home", [75] = "PageUp", [76] = "Delete",
    [77] = "End", [78] = "PageDown",
    [79] = "Right", [80] = "Left", [81] = "Down", [82] = "Up",

    -- Numpad
    [84] = "KP_Divide", [85] = "KP_Multiply", [86] = "KP_Minus",
    [87] = "KP_Plus", [88] = "KP_Enter",
    [89] = "KP_1", [90] = "KP_2", [91] = "KP_3",
    [92] = "KP_4", [93] = "KP_5", [94] = "KP_6",
    [95] = "KP_7", [96] = "KP_8", [97] = "KP_9",
    [98] = "KP_0", [99] = "KP_Period",

    -- Modifiers
    [224] = "LCtrl", [225] = "LShift", [226] = "LAlt", [227] = "LGui",
    [228] = "RCtrl", [229] = "RShift", [230] = "RAlt", [231] = "RGui",
}

-- Build reverse mapping (name -> code)
local nameToCode = {}
for code, name in pairs(codeToName) do
    nameToCode[name] = code
    nameToCode[name:lower()] = code  -- Also support lowercase
end

-- =============================================================================
-- Public API
-- =============================================================================

--- Get human-readable name for a scan code
-- @param scancode SDL scan code
-- @return name string or "Unknown" if not mapped
function Keys.getName(scancode)
    return codeToName[scancode] or string.format("Unknown(%d)", scancode)
end

--- Get scan code for a key name
-- @param name key name (case-insensitive)
-- @return scan code or nil if not found
function Keys.getCode(name)
    return nameToCode[name] or nameToCode[name:lower()]
end

--- Check if a key name represents a printable character
-- @param name key name
-- @return true if printable (letter, number, punctuation)
function Keys.isPrintable(scancode)
    -- Letters, numbers, space, and punctuation
    return (scancode >= 4 and scancode <= 39) or   -- A-Z, 0-9
           scancode == 44 or                        -- Space
           (scancode >= 45 and scancode <= 56)      -- Punctuation
end

--- Get the character for a printable key (without modifiers)
-- @param scancode SDL scan code
-- @return character string or nil if not printable
function Keys.getChar(scancode)
    local name = codeToName[scancode]
    if not name then return nil end

    -- Single character names are the character
    if #name == 1 then
        return name:lower()
    end

    -- Special cases
    if scancode == 44 then return " " end      -- Space
    if scancode == 45 then return "-" end      -- Minus
    if scancode == 46 then return "=" end      -- Equals
    if scancode == 47 then return "[" end      -- LeftBracket
    if scancode == 48 then return "]" end      -- RightBracket
    if scancode == 49 then return "\\" end     -- Backslash
    if scancode == 51 then return ";" end      -- Semicolon
    if scancode == 52 then return "'" end      -- Apostrophe
    if scancode == 53 then return "`" end      -- Grave
    if scancode == 54 then return "," end      -- Comma
    if scancode == 55 then return "." end      -- Period
    if scancode == 56 then return "/" end      -- Slash

    return nil
end

--- Get the shifted character for a printable key
-- @param scancode SDL scan code
-- @return shifted character string or nil
function Keys.getShiftedChar(scancode)
    local name = codeToName[scancode]
    if not name then return nil end

    -- Single letter names -> uppercase
    if #name == 1 and name:match("[A-Z]") then
        return name
    end

    -- Number row shifted
    local shiftMap = {
        [30] = "!", [31] = "@", [32] = "#", [33] = "$", [34] = "%",
        [35] = "^", [36] = "&", [37] = "*", [38] = "(", [39] = ")",
        [45] = "_", [46] = "+", [47] = "{", [48] = "}", [49] = "|",
        [51] = ":", [52] = "\"", [53] = "~", [54] = "<", [55] = ">",
        [56] = "?",
    }

    return shiftMap[scancode]
end

--- Get all dispatchable scancodes (all keys including modifiers)
-- @return array of all scancodes that should be checked for key events
function Keys.getAllScancodes()
    local scancodes = {}
    for code, _ in pairs(codeToName) do
        table.insert(scancodes, code)
    end
    return scancodes
end

--- Check if a scancode is a modifier key
-- @param scancode SDL scan code
-- @return true if this is a modifier key (Ctrl, Shift, Alt, Gui)
function Keys.isModifier(scancode)
    return scancode >= 224 and scancode <= 231
end

return Keys
