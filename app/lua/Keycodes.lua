--[[
    Keycodes.lua - Raylib Key Code Definitions and Mapping
]]

local Keys = {}

-- Raylib Key Codes
Keys.KEY_SPACE = 32
Keys.KEY_APOSTROPHE = 39
Keys.KEY_COMMA = 44
Keys.KEY_MINUS = 45
Keys.KEY_PERIOD = 46
Keys.KEY_SLASH = 47
Keys.KEY_ZERO = 48
Keys.KEY_ONE = 49
Keys.KEY_TWO = 50
Keys.KEY_THREE = 51
Keys.KEY_FOUR = 52
Keys.KEY_FIVE = 53
Keys.KEY_SIX = 54
Keys.KEY_SEVEN = 55
Keys.KEY_EIGHT = 56
Keys.KEY_NINE = 57
Keys.KEY_SEMICOLON = 59
Keys.KEY_EQUAL = 61
Keys.KEY_A = 65
Keys.KEY_B = 66
Keys.KEY_C = 67
Keys.KEY_D = 68
Keys.KEY_E = 69
Keys.KEY_F = 70
Keys.KEY_G = 71
Keys.KEY_H = 72
Keys.KEY_I = 73
Keys.KEY_J = 74
Keys.KEY_K = 75
Keys.KEY_L = 76
Keys.KEY_M = 77
Keys.KEY_N = 78
Keys.KEY_O = 79
Keys.KEY_P = 80
Keys.KEY_Q = 81
Keys.KEY_R = 82
Keys.KEY_S = 83
Keys.KEY_T = 84
Keys.KEY_U = 85
Keys.KEY_V = 86
Keys.KEY_W = 87
Keys.KEY_X = 88
Keys.KEY_Y = 89
Keys.KEY_Z = 90
Keys.KEY_LEFT_BRACKET = 91
Keys.KEY_BACKSLASH = 92
Keys.KEY_RIGHT_BRACKET = 93
Keys.KEY_GRAVE = 96

-- Function keys
Keys.KEY_ESCAPE = 256
Keys.KEY_ENTER = 257
Keys.KEY_TAB = 258
Keys.KEY_BACKSPACE = 259
Keys.KEY_INSERT = 260
Keys.KEY_DELETE = 261
Keys.KEY_RIGHT = 262
Keys.KEY_LEFT = 263
Keys.KEY_DOWN = 264
Keys.KEY_UP = 265
Keys.KEY_PAGE_UP = 266
Keys.KEY_PAGE_DOWN = 267
Keys.KEY_HOME = 268
Keys.KEY_END = 269
Keys.KEY_CAPS_LOCK = 280
Keys.KEY_SCROLL_LOCK = 281
Keys.KEY_NUM_LOCK = 282
Keys.KEY_PRINT_SCREEN = 283
Keys.KEY_PAUSE = 284
Keys.KEY_F1 = 290
Keys.KEY_F2 = 291
Keys.KEY_F3 = 292
Keys.KEY_F4 = 293
Keys.KEY_F5 = 294
Keys.KEY_F6 = 295
Keys.KEY_F7 = 296
Keys.KEY_F8 = 297
Keys.KEY_F9 = 298
Keys.KEY_F10 = 299
Keys.KEY_F11 = 300
Keys.KEY_F12 = 301

-- Modifiers
Keys.KEY_LEFT_SHIFT = 340
Keys.KEY_LEFT_CONTROL = 341
Keys.KEY_LEFT_ALT = 342
Keys.KEY_LEFT_SUPER = 343
Keys.KEY_RIGHT_SHIFT = 344
Keys.KEY_RIGHT_CONTROL = 345
Keys.KEY_RIGHT_ALT = 346
Keys.KEY_RIGHT_SUPER = 347
Keys.KEY_KB_MENU = 348

local codeToName = {
    [32] = "SPACE", [39] = "APOSTROPHE", [44] = "COMMA", [45] = "MINUS",
    [46] = "PERIOD", [47] = "SLASH",
    [48] = "0", [49] = "1", [50] = "2", [51] = "3", [52] = "4",
    [53] = "5", [54] = "6", [55] = "7", [56] = "8", [57] = "9",
    [59] = "SEMICOLON", [61] = "EQUAL",
    [65] = "A", [66] = "B", [67] = "C", [68] = "D", [69] = "E",
    [70] = "F", [71] = "G", [72] = "H", [73] = "I", [74] = "J",
    [75] = "K", [76] = "L", [77] = "M", [78] = "N", [79] = "O",
    [80] = "P", [81] = "Q", [82] = "R", [83] = "S", [84] = "T",
    [85] = "U", [86] = "V", [87] = "W", [88] = "X", [89] = "Y", [90] = "Z",
    [91] = "LEFT_BRACKET", [92] = "BACKSLASH", [93] = "RIGHT_BRACKET", [96] = "GRAVE",
    
    [256] = "ESCAPE", [257] = "ENTER", [258] = "TAB", [259] = "BACKSPACE",
    [260] = "INSERT", [261] = "DELETE",
    [262] = "RIGHT", [263] = "LEFT", [264] = "DOWN", [265] = "UP",
    [266] = "PAGE_UP", [267] = "PAGE_DOWN", [268] = "HOME", [269] = "END",
    [280] = "CAPS_LOCK", [281] = "SCROLL_LOCK", [282] = "NUM_LOCK",
    [283] = "PRINT_SCREEN", [284] = "PAUSE",
    [290] = "F1", [291] = "F2", [292] = "F3", [293] = "F4", [294] = "F5",
    [295] = "F6", [296] = "F7", [297] = "F8", [298] = "F9", [299] = "F10",
    [300] = "F11", [301] = "F12",
}

function Keys.getName(code)
    return codeToName[code] or string.format("Unknown(%d)", code)
end

function Keys.isModifier(code)
    return code >= 340 and code <= 348
end

return Keys
