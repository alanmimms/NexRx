local Stick = require("ui.Stick")
local Layout = {}

-- Helper to create a callable layout table with metadata
local function makeLayout(axis, fn)
  return setmetatable({ axis = axis }, { __call = function(_, ...) return fn(...) end })
end

-- Layout kids in container flowing along the horizontal axis.
Layout.hFlow = makeLayout("horizontal", function(container)
  local totalFixedW = 0
  local totalFlexW = 0

  -- Pass 1: Measure
  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local marginW = m.margin.left + m.margin.right
    if (m.flexW > 0) then
      totalFlexW = totalFlexW + m.flexW
      totalFixedW = totalFixedW + marginW
    else
      totalFixedW = totalFixedW + m.prefW + marginW
    end
  end

  local availW, availH = container.props.w, container.props.h
  local flexSpace = math.max(0, availW - totalFixedW)
  local currentX = 0

  -- Pass 2: Position
  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local stick = m.stick

    local slotW = m.prefW
    if (m.flexW > 0) and totalFlexW > 0 then
      slotW = flexSpace * (m.flexW / totalFlexW)
    end
    
    local finalW = m.prefW
    if (m.flexW > 0) or ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      finalW = slotW
    end
    finalW = math.max(m.minW, math.min(finalW, m.maxW))
    
    local kidX = currentX + m.margin.left
    local remW = slotW - finalW

    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = kidX + (remW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = kidX + remW
    end

    -- Cross axis (Y) - Aligned WITHIN the container's height
    local finalH = m.prefH
    if ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      finalH = availH - m.margin.top - m.margin.bottom
    end
    finalH = math.max(m.minH, math.min(finalH, m.maxH))
    
    local kidY = m.margin.top
    local remH = availH - m.margin.top - m.margin.bottom - finalH

    if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = kidY + (remH / 2)
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = kidY + remH
    end

    kid:layout(kidX, kidY, finalW, finalH)
    currentX = currentX + slotW + m.margin.left + m.margin.right
  end
end)

-- Layout kids in container flowing along the vertical axis.
Layout.vFlow = makeLayout("vertical", function(container)
  local totalFixedH = 0
  local totalFlexH = 0

  -- Pass 1: Measure
  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local marginH = m.margin.top + m.margin.bottom
    if (m.flexH > 0) then
      totalFlexH = totalFlexH + m.flexH
      totalFixedH = totalFixedH + marginH
    else
      totalFixedH = totalFixedH + m.prefH + marginH
    end
  end

  local availW, availH = container.props.w, container.props.h
  local flexSpace = math.max(0, availH - totalFixedH)
  local currentY = 0

  -- Pass 2: Position
  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local stick = m.stick

    local slotH = m.prefH
    if (m.flexH > 0) and totalFlexH > 0 then
      slotH = flexSpace * (m.flexH / totalFlexH)
    end
    
    local finalH = m.prefH
    if (m.flexH > 0) or ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      finalH = slotH
    end
    finalH = math.max(m.minH, math.min(finalH, m.maxH))
    
    local kidY = currentY + m.margin.top
    local remH = slotH - finalH

    if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = kidY + (remH / 2)
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = kidY + remH
    end

    -- Cross axis (X) - Aligned WITHIN the container's width
    local finalW = m.prefW
    if ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      finalW = availW - m.margin.left - m.margin.right
    end
    finalW = math.max(m.minW, math.min(finalW, m.maxW))
    
    local kidX = m.margin.left
    local remW = availW - m.margin.left - m.margin.right - finalW

    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = kidX + (remW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = kidX + remW
    end

    kid:layout(kidX, kidY, finalW, finalH)
    currentY = currentY + slotH + m.margin.top + m.margin.bottom
  end
end)

Layout.zStack = makeLayout("stack", function(container)
  local availW, availH = container.props.w, container.props.h

  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local stick = m.stick

    local finalW = m.prefW
    if ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      finalW = availW - m.margin.left - m.margin.right
    end
    finalW = math.max(m.minW, math.min(finalW, m.maxW))

    local finalH = m.prefH
    if ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      finalH = availH - m.margin.top - m.margin.bottom
    end
    finalH = math.max(m.minH, math.min(finalH, m.maxH))

    local kidX = m.margin.left
    local kidY = m.margin.top

    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = (availW / 2) - (finalW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = availW - finalW - m.margin.right
    end

    if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = (availH / 2) - (finalH / 2)
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = availH - finalH - m.margin.bottom
    end

    kid:layout(kidX, kidY, finalW, finalH)
  end
end)

Layout.wrapFlow = makeLayout("wrap", function(container)
  local availW = container.props.w
  local currentX, currentY = 0, 0
  local rowMaxH = 0

  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local kidTotalW = m.prefW + m.margin.left + m.margin.right
    local kidTotalH = m.prefH + m.margin.top + m.margin.bottom

    if (currentX + kidTotalW > availW and currentX > 0) then
      currentX = 0
      currentY = currentY + rowMaxH
      rowMaxH = 0
    end

    local finalW = math.max(m.minW, math.min(m.prefW, m.maxW))
    local finalH = math.max(m.minH, math.min(m.prefH, m.maxH))
    kid:layout(currentX + m.margin.left, currentY + m.margin.top, finalW, finalH)
    
    currentX = currentX + kidTotalW
    if (kidTotalH > rowMaxH) then rowMaxH = kidTotalH end
  end
end)

Layout.grid = makeLayout("grid", function(container)
  local availW, availH = container.props.w, container.props.h
  local cols = container.props.cols or 1
  local nKids = #container.kids
  local rows = math.ceil(nKids / cols)

  local colWidths = {}
  local rowHeights = {}
  for i = 1, cols do colWidths[i] = 0 end
  for i = 1, rows do rowHeights[i] = 0 end

  for i, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local col = ((i - 1) % cols) + 1
    local row = math.floor((i - 1) / cols) + 1
    local totalW = m.prefW + m.margin.left + m.margin.right
    local totalH = m.prefH + m.margin.top + m.margin.bottom
    if (totalW > colWidths[col]) then colWidths[col] = totalW end
    if (totalH > rowHeights[row]) then rowHeights[row] = totalH end
  end

  local currentY = 0
  for row = 1, rows do
    local currentX = 0
    for col = 1, cols do
      local kidIdx = (row - 1) * cols + col
      if (kidIdx > nKids) then break end
      
      local kid = container.kids[kidIdx]
      local m = kid:getMetrics()
      local stick = m.stick
      local cellW, cellH = colWidths[col], rowHeights[row]

      local finalW = m.prefW
      if (stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0 then finalW = cellW - m.margin.left - m.margin.right end
      finalW = math.max(m.minW, math.min(finalW, m.maxW))

      local finalH = m.prefH
      if (stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0 then finalH = cellH - m.margin.top - m.margin.bottom end
      finalH = math.max(m.minH, math.min(finalH, m.maxH))

      local kidX = currentX + m.margin.left
      local kidY = currentY + m.margin.top

      if (stick & Stick.L) == 0 and (stick & Stick.R) == 0 then kidX = kidX + (cellW - (finalW + m.margin.left + m.margin.right)) / 2
      elseif (stick & Stick.L) == 0 and (stick & Stick.R) ~= 0 then kidX = currentX + cellW - finalW - m.margin.right end

      if (stick & Stick.T) == 0 and (stick & Stick.B) == 0 then kidY = kidY + (cellH - (finalH + m.margin.top + m.margin.bottom)) / 2
      elseif (stick & Stick.T) == 0 and (stick & Stick.B) ~= 0 then kidY = currentY + cellH - finalH - m.margin.bottom end

      kid:layout(kidX, kidY, finalW, finalH)
      currentX = currentX + cellW
    end
    currentY = currentY + rowHeights[row]
  end
end)

return Layout
