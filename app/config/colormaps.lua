--[[
  NexRx Colormap Definitions

  Colormaps are defined as gradient stops: a list of {position, r, g, b}
  where position is 0.0-1.0 and RGB values are 0-255.

  Users can add custom colormaps by defining additional entries.
  The colormap name is used in config to select it.
]]

colormaps = {
    -- Viridis: Blue-green-yellow (perceptually uniform)
    viridis = {
        {0.00, 68, 1, 84},
        {0.25, 59, 82, 139},
        {0.50, 33, 145, 140},
        {0.75, 94, 201, 98},
        {1.00, 253, 231, 37},
    },

    -- Plasma: Purple-red-yellow
    plasma = {
        {0.00, 13, 8, 135},
        {0.25, 126, 3, 168},
        {0.50, 204, 71, 120},
        {0.75, 248, 149, 64},
        {1.00, 240, 249, 33},
    },

    -- Inferno: Black-red-yellow-white
    inferno = {
        {0.00, 0, 0, 4},
        {0.25, 87, 16, 110},
        {0.50, 188, 55, 84},
        {0.75, 249, 142, 9},
        {1.00, 252, 255, 164},
    },

    -- Magma: Black-purple-orange-white
    magma = {
        {0.00, 0, 0, 4},
        {0.25, 81, 18, 124},
        {0.50, 183, 55, 121},
        {0.75, 254, 159, 109},
        {1.00, 252, 253, 191},
    },

    -- Green Phosphor: Classic CRT oscilloscope look
    green = {
        {0.00, 0, 10, 0},
        {0.30, 0, 60, 0},
        {0.60, 0, 180, 0},
        {0.80, 100, 255, 100},
        {1.00, 200, 255, 200},
    },

    -- Blue Hot: Blue to white (good for weak signals)
    blue = {
        {0.00, 0, 0, 20},
        {0.30, 0, 50, 150},
        {0.60, 50, 150, 255},
        {0.80, 200, 220, 255},
        {1.00, 255, 255, 255},
    },

    -- Grayscale: Simple black to white
    grayscale = {
        {0.00, 0, 0, 0},
        {1.00, 255, 255, 255},
    },

    -- Turbo: Rainbow colormap (high contrast)
    turbo = {
        {0.00, 48, 18, 59},
        {0.20, 70, 117, 237},
        {0.40, 49, 198, 166},
        {0.60, 147, 231, 81},
        {0.80, 249, 185, 47},
        {1.00, 122, 4, 3},
    },

    -- Hot: Black-red-yellow-white (thermal)
    hot = {
        {0.00, 0, 0, 0},
        {0.33, 230, 0, 0},
        {0.66, 255, 200, 0},
        {1.00, 255, 255, 255},
    },

    -- Cool: Cyan to magenta
    cool = {
        {0.00, 0, 255, 255},
        {1.00, 255, 0, 255},
    },

    -- Amber: Classic amber phosphor (vintage CRT)
    amber = {
        {0.00, 10, 5, 0},
        {0.30, 80, 40, 0},
        {0.60, 200, 120, 0},
        {0.80, 255, 180, 50},
        {1.00, 255, 230, 150},
    },
}

-- Helper function to get colormap by name
function getColormap(name)
    return colormaps[name] or colormaps.viridis
end

-- Get list of available colormap names
function getColormapNames()
    local names = {}
    for name in pairs(colormaps) do
        table.insert(names, name)
    end
    table.sort(names)
    return names
end

print("[colormaps.lua] Colormap definitions loaded")
