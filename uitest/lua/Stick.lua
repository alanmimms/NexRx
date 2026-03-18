local Stick = {
  L = 1, R = 2, T = 4, B = 8,
  LR = 3,
  TB = 12,
  TLBR = 15,
  TL = 5,
  TR = 6,
  BL = 9,
  BR = 10,
  TLR = 7,
  BLR = 11,
  TLB = 13,
  TRB = 14,
}

function Stick.mk(s)
  if s == "all" then
    return 15
  end

  local mask = 0
  
  for c in s:gmatch(".") do
    mask = mask | (Stick[c] or 0)
  end

  return mask
end

return Stick
