local Color = {}
Color.__index = Color

function Color.new(r, g, b, a)
  local self = setmetatable({}, Color)
  if type(r) == "table" then
    self.r = r.r or r[1] or 0
    self.g = r.g or r[2] or 0
    self.b = r.b or r[3] or 0
    self.a = r.a or r[4] or 1.0
  elseif type(r) == "string" and r:sub(1, 1) == "#" then
    local hex = r:gsub("#", "")
    local len = #hex
    if len == 3 or len == 4 then
      local r_hex = hex:sub(1, 1)
      local g_hex = hex:sub(2, 2)
      local b_hex = hex:sub(3, 3)
      self.r = tonumber(r_hex .. r_hex, 16) / 255.0
      self.g = tonumber(g_hex .. g_hex, 16) / 255.0
      self.b = tonumber(b_hex .. b_hex, 16) / 255.0
      if len == 4 then
        local a_hex = hex:sub(4, 4)
        self.a = tonumber(a_hex .. a_hex, 16) / 255.0
      else
        self.a = 1.0
      end
    elseif len == 6 or len == 8 then
      self.r = (tonumber(hex:sub(1, 2), 16) or 0) / 255.0
      self.g = (tonumber(hex:sub(3, 4), 16) or 0) / 255.0
      self.b = (tonumber(hex:sub(5, 6), 16) or 0) / 255.0
      if len == 8 then
        self.a = (tonumber(hex:sub(7, 8), 16) or 0) / 255.0
      else
        self.a = 1.0
      end
    end
  else
    self.r = r or 0
    self.g = g or 0
    self.b = b or 0
    self.a = a or 1.0
  end
  return self
end

setmetatable(Color, {
  __call = function(_, ...)
    return Color.new(...)
  end
})

function Color:toTable()
  return { self.r, self.g, self.b, self.a }
end

function Color:darken(factor)
  return Color.new(self.r * (1.0 - factor), self.g * (1.0 - factor), self.b * (1.0 - factor), self.a)
end

return Color
