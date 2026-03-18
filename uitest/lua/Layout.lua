local Stick = require("Stick")
local Layout = {}

-- Layout kids in container flowing along the horizontal axis.
function Layout.hFlow(container)
  local totalFixedW = 0
  local totalFlexW = 0

  -- Pass 1: Measure fixed space and sum flex weights
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

  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
  local flexSpace = math.max(0, availW - totalFixedW)
  local currentX = startX

  -- Pass 2: Calculate final geometry and apply stickiness
  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local stick = m.stick

    -- 1. Determine Width and Main Axis (X) Slot
    local slotW = m.prefW
    if (m.flexW > 0) then
      slotW = flexSpace * (m.flexW / totalFlexW)
    end
    
    local finalW = m.prefW
    if (m.flexW > 0) or ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      finalW = slotW
    end
    finalW = math.max(m.minW, math.min(finalW, m.maxW))
    
    local kidX = currentX + m.margin.left
    local remW = slotW - finalW

    -- X-Axis Alignment within the slot
    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = kidX + (remW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = kidX + remW
    end

    -- 2. Determine Height and Cross Axis (Y) Slot
    local finalH = m.prefH
    if ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      finalH = availH - m.margin.top - m.margin.bottom
    end
    finalH = math.max(m.minH, math.min(finalH, m.maxH))
    
    local kidY = startY + m.margin.top
    local remH = availH - m.margin.top - m.margin.bottom - finalH

    -- Y-Axis Alignment
    if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = kidY + (remH / 2)
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = kidY + remH
    end

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
    currentX = currentX + slotW + m.margin.left + m.margin.right
  end
end


-- Layout kids in container flowing along the vertical axis.
function Layout.vFlow(container)
  local totalFixedH = 0
  local totalFlexH = 0

  -- Pass 1: Measure fixed space and sum flex weights
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

  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
  local flexSpace = math.max(0, availH - totalFixedH)
  local currentY = startY

  -- Pass 2: Calculate final geometry and apply stickiness
  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local stick = m.stick

    -- 1. Determine Height and Main Axis (Y) Slot
    local slotH = m.prefH
    if (m.flexH > 0) then
      slotH = flexSpace * (m.flexH / totalFlexH)
    end
    
    local finalH = m.prefH
    if (m.flexH > 0) or ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      finalH = slotH
    end
    finalH = math.max(m.minH, math.min(finalH, m.maxH))
    
    local kidY = currentY + m.margin.top
    local remH = slotH - finalH

    -- Y-Axis Alignment within the slot
    if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = kidY + (remH / 2)
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = kidY + remH
    end

    -- 2. Determine Width and Cross Axis (X) Slot
    local finalW = m.prefW
    if ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      finalW = availW - m.margin.left - m.margin.right
    end
    finalW = math.max(m.minW, math.min(finalW, m.maxW))
    
    local kidX = startX + m.margin.left
    local remW = availW - m.margin.left - m.margin.right - finalW

    -- X-Axis Alignment
    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = kidX + (remW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = kidX + remW
    end

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
    currentY = currentY + slotH + m.margin.top + m.margin.bottom
  end
end


function Layout.zStack(container)
  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y

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

    local kidX = startX + m.margin.left
    local kidY = startY + m.margin.top

    -- X-Axis Alignment
    if ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      -- Already set to fill
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = startX + availW - finalW - m.margin.right
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = startX + (availW / 2) - (finalW / 2)
    end

    -- Y-Axis Alignment
    if ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      -- Already set to fill
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = startY + availH - finalH - m.margin.bottom
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = startY + (availH / 2) - (finalH / 2)
    end

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
  end
end


function Layout.wrapFlow(container)
  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
  local currentX, currentY = startX, startY
  local rowMaxH = 0

  -- This one is more complex to handle "stretch" in wrap rows without a multi-pass.
  -- For now, we keep it simple: alignment but no stretching beyond prefSize.
  for _, kid in ipairs(container.kids) do
    local m = kid:getMetrics()
    local stick = m.stick
    
    local kidTotalW = m.prefW + m.margin.left + m.margin.right
    local kidTotalH = m.prefH + m.margin.top + m.margin.bottom

    if (currentX + kidTotalW > startX + availW and currentX > startX) then
      currentX = startX
      currentY = currentY + rowMaxH
      rowMaxH = 0
    end

    local finalW = math.max(m.minW, math.min(m.prefW, m.maxW))
    local finalH = math.max(m.minH, math.min(m.prefH, m.maxH))
    local kidX = currentX + m.margin.left
    local kidY = currentY + m.margin.top

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
    
    currentX = currentX + kidTotalW
    if (kidTotalH > rowMaxH) then rowMaxH = kidTotalH end
  end
end


function Layout.grid(container)
  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
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

  local currentY = startY
  for row = 1, rows do
    local currentX = startX
    for col = 1, cols do
      local kidIdx = (row - 1) * cols + col
      if (kidIdx > nKids) then break end
      
      local kid = container.kids[kidIdx]
      local m = kid:getMetrics()
      local stick = m.stick
      local cellW = colWidths[col]
      local cellH = rowHeights[row]

      local finalW = m.prefW
      if (stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0 then finalW = cellW - m.margin.left - m.margin.right end
      finalW = math.max(m.minW, math.min(finalW, m.maxW))

      local finalH = m.prefH
      if (stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0 then finalH = cellH - m.margin.top - m.margin.bottom end
      finalH = math.max(m.minH, math.min(finalH, m.maxH))

      local kidX = currentX + m.margin.left
      local kidY = currentY + m.margin.top

      -- Alignment
      if (stick & Stick.L) == 0 and (stick & Stick.R) == 0 then kidX = kidX + (cellW - finalW) / 2
      elseif (stick & Stick.L) == 0 and (stick & Stick.R) ~= 0 then kidX = kidX + (cellW - finalW) end

      if (stick & Stick.T) == 0 and (stick & Stick.B) == 0 then kidY = kidY + (cellH - finalH) / 2
      elseif (stick & Stick.T) == 0 and (stick & Stick.B) ~= 0 then kidY = kidY + (cellH - finalH) end

      kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
      currentX = currentX + cellW
    end
    currentY = currentY + rowHeights[row]
  end
end

return Layout
