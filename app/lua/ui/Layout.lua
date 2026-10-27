local Stick = require("ui.Stick")
local Layout = {}

-- Helper to create a callable layout table with metadata
local function makeLayout(axis, fn)
  return setmetatable({ axis = axis }, { __call = function(_, ...) return fn(...) end })
end

local function getMargin(p)
  local m = p.margin
  if type(m) == "number" then
    return { left = m, right = m, top = m, bottom = m }
  elseif type(m) == "table" then
    return {
      left = m.left or 0,
      right = m.right or 0,
      top = m.top or 0,
      bottom = m.bottom or 0,
    }
  end
  return { left = 0, right = 0, top = 0, bottom = 0 }
end

local function getStick(p)
  local s = p.stick
  if type(s) == "string" then
    return Stick.mk(s)
  elseif type(s) == "number" then
    return s
  end
  return Stick.L | Stick.T
end

-- Layout kids in container flowing along the horizontal axis.
Layout.hFlow = makeLayout("horizontal", function(container)
  local totalFixedW = 0
  local totalFlexW = 0

  -- Pass 1: Measure
  for _, kid in ipairs(container.kids) do
    local p = kid.props
    local margin = getMargin(p)
    local marginW = margin.left + margin.right
    local flexW = p.flexW or 0
    local prefW = p.prefW or 0
    if flexW > 0 then
      totalFlexW = totalFlexW + flexW
      totalFixedW = totalFixedW + marginW
    else
      totalFixedW = totalFixedW + prefW + marginW
    end
  end

  local availW, availH = container.props.w or 0, container.props.h or 0
  local flexSpace = math.max(0, availW - totalFixedW)
  local currentX = 0

  -- Pass 2: Position
  for _, kid in ipairs(container.kids) do
    local p = kid.props
    local margin = getMargin(p)
    local stick = getStick(p)
    local flexW = p.flexW or 0
    local prefW = p.prefW or 0
    local prefH = p.prefH or 0
    local minW = p.minW or 0
    local maxW = p.maxW or 10000
    local minH = p.minH or 0
    local maxH = p.maxH or 10000

    local slotW = prefW
    if flexW > 0 and totalFlexW > 0 then
      slotW = flexSpace * (flexW / totalFlexW)
    end

    local finalW = prefW
    if flexW > 0 or ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      finalW = slotW
    end
    finalW = math.max(minW, math.min(finalW, maxW))

    local kidX = currentX + margin.left
    local remW = slotW - finalW

    if (stick & Stick.L) == 0 and (stick & Stick.R) == 0 then
      kidX = kidX + (remW / 2)
    elseif (stick & Stick.L) == 0 and (stick & Stick.R) ~= 0 then
      kidX = kidX + remW
    end

    -- Cross axis (Y) - Aligned WITHIN the container's height
    local finalH = prefH
    if (stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0 then
      finalH = availH - margin.top - margin.bottom
    end
    finalH = math.max(minH, math.min(finalH, maxH))

    local kidY = margin.top
    local remH = availH - margin.top - margin.bottom - finalH

    if (stick & Stick.T) == 0 and (stick & Stick.B) == 0 then
      kidY = kidY + (remH / 2)
    elseif (stick & Stick.T) == 0 and (stick & Stick.B) ~= 0 then
      kidY = kidY + remH
    end

    kid:layout(kidX, kidY, finalW, finalH)
    currentX = currentX + slotW + margin.left + margin.right
  end
end)

-- Layout kids in container flowing along the vertical axis.
Layout.vFlow = makeLayout("vertical", function(container)
  local totalFixedH = 0
  local totalFlexH = 0

  -- Pass 1: Measure
  for _, kid in ipairs(container.kids) do
    local p = kid.props
    local margin = getMargin(p)
    local marginH = margin.top + margin.bottom
    local flexH = p.flexH or 0
    local prefH = p.prefH or 0
    if flexH > 0 then
      totalFlexH = totalFlexH + flexH
      totalFixedH = totalFixedH + marginH
    else
      totalFixedH = totalFixedH + prefH + marginH
    end
  end

  local availW, availH = container.props.w or 0, container.props.h or 0
  local flexSpace = math.max(0, availH - totalFixedH)
  local currentY = 0

  -- Pass 2: Position
  for _, kid in ipairs(container.kids) do
    local p = kid.props
    local margin = getMargin(p)
    local stick = getStick(p)
    local flexH = p.flexH or 0
    local prefW = p.prefW or 0
    local prefH = p.prefH or 0
    local minW = p.minW or 0
    local maxW = p.maxW or 10000
    local minH = p.minH or 0
    local maxH = p.maxH or 10000

    local slotH = prefH
    if flexH > 0 and totalFlexH > 0 then
      slotH = flexSpace * (flexH / totalFlexH)
    end

    local finalH = prefH
    if flexH > 0 or ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      finalH = slotH
    end
    finalH = math.max(minH, math.min(finalH, maxH))

    local kidY = currentY + margin.top
    local remH = slotH - finalH

    if (stick & Stick.T) == 0 and (stick & Stick.B) == 0 then
      kidY = kidY + (remH / 2)
    elseif (stick & Stick.T) == 0 and (stick & Stick.B) ~= 0 then
      kidY = kidY + remH
    end

    -- Cross axis (X) - Aligned WITHIN the container's width
    local finalW = prefW
    if (stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0 then
      finalW = availW - margin.left - margin.right
    end
    finalW = math.max(minW, math.min(finalW, maxW))

    local kidX = margin.left
    local remW = availW - margin.left - margin.right - finalW

    if (stick & Stick.L) == 0 and (stick & Stick.R) == 0 then
      kidX = kidX + (remW / 2)
    elseif (stick & Stick.L) == 0 and (stick & Stick.R) ~= 0 then
      kidX = kidX + remW
    end

    kid:layout(kidX, kidY, finalW, finalH)
    currentY = currentY + slotH + margin.top + margin.bottom
  end
end)

Layout.zStack = makeLayout("stack", function(container)
  local availW, availH = container.props.w or 0, container.props.h or 0

  for _, kid in ipairs(container.kids) do
    local p = kid.props
    local margin = getMargin(p)
    local stick = getStick(p)
    local prefW = p.prefW or 0
    local prefH = p.prefH or 0
    local minW = p.minW or 0
    local maxW = p.maxW or 10000
    local minH = p.minH or 0
    local maxH = p.maxH or 10000

    local finalW = prefW
    if (stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0 then
      finalW = availW - margin.left - margin.right
    end
    finalW = math.max(minW, math.min(finalW, maxW))

    local finalH = prefH
    if (stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0 then
      finalH = availH - margin.top - margin.bottom
    end
    finalH = math.max(minH, math.min(finalH, maxH))

    local kidX = margin.left
    local kidY = margin.top

    if (stick & Stick.L) == 0 and (stick & Stick.R) == 0 then
      kidX = (availW / 2) - (finalW / 2)
    elseif (stick & Stick.L) == 0 and (stick & Stick.R) ~= 0 then
      kidX = availW - finalW - margin.right
    end

    if (stick & Stick.T) == 0 and (stick & Stick.B) == 0 then
      kidY = (availH / 2) - (finalH / 2)
    elseif (stick & Stick.T) == 0 and (stick & Stick.B) ~= 0 then
      kidY = availH - finalH - margin.bottom
    end

    kid:layout(kidX, kidY, finalW, finalH)
  end
end)

Layout.wrapFlow = makeLayout("wrap", function(container)
  local availW = container.props.w or 0
  local currentX, currentY = 0, 0
  local rowMaxH = 0

  for _, kid in ipairs(container.kids) do
    local p = kid.props
    local margin = getMargin(p)
    local prefW = p.prefW or 0
    local prefH = p.prefH or 0
    local minW = p.minW or 0
    local maxW = p.maxW or 10000
    local minH = p.minH or 0
    local maxH = p.maxH or 10000

    local kidTotalW = prefW + margin.left + margin.right
    local kidTotalH = prefH + margin.top + margin.bottom

    if currentX + kidTotalW > availW and currentX > 0 then
      currentX = 0
      currentY = currentY + rowMaxH
      rowMaxH = 0
    end

    local finalW = math.max(minW, math.min(prefW, maxW))
    local finalH = math.max(minH, math.min(prefH, maxH))
    kid:layout(currentX + margin.left, currentY + margin.top, finalW, finalH)

    currentX = currentX + kidTotalW
    if kidTotalH > rowMaxH then rowMaxH = kidTotalH end
  end
end)

Layout.grid = makeLayout("grid", function(container)
  local cols = container.props.cols or 1
  local nKids = #container.kids
  local rows = math.ceil(nKids / cols)

  local colWidths = {}
  local rowHeights = {}
  for i = 1, cols do colWidths[i] = 0 end
  for i = 1, rows do rowHeights[i] = 0 end

  for i, kid in ipairs(container.kids) do
    local p = kid.props
    local margin = getMargin(p)
    local col = ((i - 1) % cols) + 1
    local row = math.floor((i - 1) / cols) + 1
    local totalW = (p.prefW or 0) + margin.left + margin.right
    local totalH = (p.prefH or 0) + margin.top + margin.bottom
    if totalW > colWidths[col] then colWidths[col] = totalW end
    if totalH > rowHeights[row] then rowHeights[row] = totalH end
  end

  local currentY = 0
  for row = 1, rows do
    local currentX = 0
    for col = 1, cols do
      local kidIdx = (row - 1) * cols + col
      if kidIdx > nKids then break end

      local kid = container.kids[kidIdx]
      local p = kid.props
      local margin = getMargin(p)
      local stick = getStick(p)
      local prefW = p.prefW or 0
      local prefH = p.prefH or 0
      local minW = p.minW or 0
      local maxW = p.maxW or 10000
      local minH = p.minH or 0
      local maxH = p.maxH or 10000
      local cellW, cellH = colWidths[col], rowHeights[row]

      local finalW = prefW
      if (stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0 then
        finalW = cellW - margin.left - margin.right
      end
      finalW = math.max(minW, math.min(finalW, maxW))

      local finalH = prefH
      if (stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0 then
        finalH = cellH - margin.top - margin.bottom
      end
      finalH = math.max(minH, math.min(finalH, maxH))

      local kidX = currentX + margin.left
      local kidY = currentY + margin.top

      if (stick & Stick.L) == 0 and (stick & Stick.R) == 0 then
        kidX = kidX + (cellW - (finalW + margin.left + margin.right)) / 2
      elseif (stick & Stick.L) == 0 and (stick & Stick.R) ~= 0 then
        kidX = currentX + cellW - finalW - margin.right
      end

      if (stick & Stick.T) == 0 and (stick & Stick.B) == 0 then
        kidY = kidY + (cellH - (finalH + margin.top + margin.bottom)) / 2
      elseif (stick & Stick.T) == 0 and (stick & Stick.B) ~= 0 then
        kidY = currentY + cellH - finalH - margin.bottom
      end

      kid:layout(kidX, kidY, finalW, finalH)
      currentX = currentX + cellW
    end
    currentY = currentY + rowHeights[row]
  end
end)

return Layout
