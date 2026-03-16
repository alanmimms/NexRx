local Layout = {}

-- Layout kids in container flowing along the horizontal axis.
function Layout.layoutHFlow(container)
  local totalFixedW = 0
  local totalFlexW = 0

  -- Pass 1: Measure fixed space and sum flex weights
  for _, kid in ipairs(container.kids) do
    local metrics = kid:getMetrics()
    local marginW = metrics.margin.left + metrics.margin.right
    
    if (metrics.flexW > 0) then
      totalFlexW = totalFlexW + metrics.flexW
      totalFixedW = totalFixedW + marginW
    else
      totalFixedW = totalFixedW + metrics.prefW + marginW
    end
  end

  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
  local flexSpace = math.max(0, availW - totalFixedW)
  local currentX = startX

  -- Pass 2: Calculate final geometry and apply stickiness
  for _, kid in ipairs(container.kids) do
    local metrics = kid:getMetrics()
    local stick = metrics.stick

    -- 1. Determine Width and Main Axis (X) Slot
    local slotW = metrics.prefW
    if (metrics.flexW > 0) then
      slotW = flexSpace * (metrics.flexW / totalFlexW)
    end
    
    local finalW = math.max(metrics.minW, math.min(slotW, metrics.maxW))
    local kidX = currentX + metrics.margin.left
    local remW = slotW - finalW

    -- X-Axis Alignment
    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = kidX + (remW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = kidX + remW
    end

    -- 2. Determine Height and Cross Axis (Y) Slot
    local finalH = math.max(metrics.minH, math.min(availH - metrics.margin.top - metrics.margin.bottom, metrics.maxH))
    local kidY = startY + metrics.margin.top
    local remH = availH - metrics.margin.top - metrics.margin.bottom - finalH

    -- Y-Axis Alignment
    if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = kidY + (remH / 2)
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = kidY + remH
    end

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
    currentX = currentX + slotW + metrics.margin.left + metrics.margin.right
  end
end


-- Layout kids in container flowing along the vertical axis.
function Layout.layoutVFlow(container)
  local totalFixedH = 0
  local totalFlexH = 0

  -- Pass 1: Measure fixed space and sum flex weights
  for _, kid in ipairs(container.kids) do
    local metrics = kid:getMetrics()
    local marginH = metrics.margin.top + metrics.margin.bottom
    
    if (metrics.flexH > 0) then
      totalFlexH = totalFlexH + metrics.flexH
      totalFixedH = totalFixedH + marginH
    else
      totalFixedH = totalFixedH + metrics.prefH + marginH
    end
  end

  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
  local flexSpace = math.max(0, availH - totalFixedH)
  local currentY = startY

  -- Pass 2: Calculate final geometry and apply stickiness
  for _, kid in ipairs(container.kids) do
    local metrics = kid:getMetrics()
    local stick = metrics.stick

    -- 1. Determine Height and Main Axis (Y) Slot
    local slotH = metrics.prefH
    if (metrics.flexH > 0) then
      slotH = flexSpace * (metrics.flexH / totalFlexH)
    end
    
    local finalH = math.max(metrics.minH, math.min(slotH, metrics.maxH))
    local kidY = currentY + metrics.margin.top
    local remH = slotH - finalH

    -- Y-Axis Alignment
    if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = kidY + (remH / 2)
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = kidY + remH
    end

    -- 2. Determine Width and Cross Axis (X) Slot
    local finalW = math.max(metrics.minW, math.min(availW - metrics.margin.left - metrics.margin.right, metrics.maxW))
    local kidX = startX + metrics.margin.left
    local remW = availW - metrics.margin.left - metrics.margin.right - finalW

    -- X-Axis Alignment
    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = kidX + (remW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = kidX + remW
    end

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
    currentY = currentY + slotH + metrics.margin.top + metrics.margin.bottom
  end
end


-- Z-Stack / Absolute Layout: A container where kids do not flow
-- sequentially to make room for each other, but simply stack directly
-- on top of one another at absolute coordinates relative to the
-- parent, or stretch to fill the parent based entirely on their
-- stickiness. This is heavily used for overlays, modal backdrops, or
-- placing floating tooltips.
function Layout.layoutZStack(container)
  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y

  for _, kid in ipairs(container.kids) do
    local metrics = kid:getMetrics()
    local stick = metrics.stick

    local finalW = math.max(metrics.minW, math.min(metrics.prefW, metrics.maxW))
    local finalH = math.max(metrics.minH, math.min(metrics.prefH, metrics.maxH))
    local kidX = startX + metrics.margin.left
    local kidY = startY + metrics.margin.top

    -- X-Axis Alignment and Stretching relative to container bounds
    if ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
      finalW = availW - metrics.margin.left - metrics.margin.right
      finalW = math.max(metrics.minW, math.min(finalW, metrics.maxW))
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = startX + availW - finalW - metrics.margin.right
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = startX + (availW / 2) - (finalW / 2)
    end

    -- Y-Axis Alignment and Stretching relative to container bounds
    if ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
      finalH = availH - metrics.margin.top - metrics.margin.bottom
      finalH = math.max(metrics.minH, math.min(finalH, metrics.maxH))
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
      kidY = startY + availH - finalH - metrics.margin.bottom
    elseif ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
      kidY = startY + (availH / 2) - (finalH / 2)
    end

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
  end
end

-- Wrap Flow: A horizontal flow that, when it runs out of availW,
-- moves the currentY cursor down by the tallest kid in the previous
-- row and continues flowing horizontally (like words in a paragraph).


function Layout.layoutWrapFlow(container)
  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
  local currentX, currentY = startX, startY
  local rowMaxH = 0

  for _, kid in ipairs(container.kids) do
    local metrics = kid:getMetrics()
    local stick = metrics.stick
    
    local kidTotalW = metrics.prefW + metrics.margin.left + metrics.margin.right
    local kidTotalH = metrics.prefH + metrics.margin.top + metrics.margin.bottom

    -- Wrap to the next line if we exceed container width
    if (currentX + kidTotalW > startX + availW and currentX > startX) then
      currentX = startX
      currentY = currentY + rowMaxH
      rowMaxH = 0
    end

    local finalW = math.max(metrics.minW, math.min(metrics.prefW, metrics.maxW))
    local finalH = math.max(metrics.minH, math.min(metrics.prefH, metrics.maxH))
    local kidX = currentX + metrics.margin.left
    local kidY = currentY + metrics.margin.top
    local remW = kidTotalW - finalW - metrics.margin.left - metrics.margin.right
    local remH = rowMaxH - finalH - metrics.margin.top - metrics.margin.bottom

    -- X-Axis Alignment within the allocated sequential flow slot
    if ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
      kidX = kidX + (remW / 2)
    elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
      kidX = kidX + remW
    end

    -- Y-Axis Alignment within the current row height limits
    if (rowMaxH > 0) then
      if ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
        kidY = kidY + (remH / 2)
      elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
        kidY = kidY + remH
      end
    end

    kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
    
    currentX = currentX + kidTotalW
    if (kidTotalH > rowMaxH) then
      rowMaxH = kidTotalH
    end
  end
end


-- Grid Layout: While you can nest HFlows inside VFlows to create a
-- grid, a true 2D Grid layout ensures that column widths align
-- uniformly across multiple rows, which nested flows cannot guarantee
-- dynamically.
function Layout.layoutGrid(container)
  local availW, availH, startX, startY = container.props.w, container.props.h, container.props.x, container.props.y
  local cols = container.props.cols or 1
  local nKids = #container.kids
  local rows = math.ceil(nKids / cols)

  local colWidths = {}
  local rowHeights = {}
  for i = 1, cols do colWidths[i] = 0 end
  for i = 1, rows do rowHeights[i] = 0 end

  -- Pass 1: Measure max intrinsic sizes per column and per row
  for i, kid in ipairs(container.kids) do
    local metrics = kid:getMetrics()
    local col = ((i - 1) % cols) + 1
    local row = math.floor((i - 1) / cols) + 1
    
    local totalW = metrics.prefW + metrics.margin.left + metrics.margin.right
    local totalH = metrics.prefH + metrics.margin.top + metrics.margin.bottom
    
    if (totalW > colWidths[col]) then colWidths[col] = totalW end
    if (totalH > rowHeights[row]) then rowHeights[row] = totalH end
  end

  -- Pass 2: Position kids in their respective grid cells
  local currentY = startY
  for row = 1, rows do
    local currentX = startX
    
    for col = 1, cols do
      local kidIdx = (row - 1) * cols + col
      if (kidIdx > nKids) then break end
      
      local kid = container.kids[kidIdx]
      local metrics = kid:getMetrics()
      local stick = metrics.stick

      local cellW = colWidths[col]
      local cellH = rowHeights[row]

      local finalW = math.max(metrics.minW, math.min(metrics.prefW, metrics.maxW))
      local finalH = math.max(metrics.minH, math.min(metrics.prefH, metrics.maxH))
      local kidX = currentX + metrics.margin.left
      local kidY = currentY + metrics.margin.top
      local remW = cellW - finalW - metrics.margin.left - metrics.margin.right
      local remH = cellH - finalH - metrics.margin.top - metrics.margin.bottom

      -- X-Axis Alignment and Stretching within the cell
      if ((stick & Stick.L) ~= 0 and (stick & Stick.R) ~= 0) then
        finalW = cellW - metrics.margin.left - metrics.margin.right
        finalW = math.max(metrics.minW, math.min(finalW, metrics.maxW))
      elseif ((stick & Stick.L) == 0 and (stick & Stick.R) == 0) then
        kidX = kidX + (remW / 2)
      elseif ((stick & Stick.L) == 0 and (stick & Stick.R) ~= 0) then
        kidX = kidX + remW
      end

      -- Y-Axis Alignment and Stretching within the cell
      if ((stick & Stick.T) ~= 0 and (stick & Stick.B) ~= 0) then
        finalH = cellH - metrics.margin.top - metrics.margin.bottom
        finalH = math.max(metrics.minH, math.min(finalH, metrics.maxH))
      elseif ((stick & Stick.T) == 0 and (stick & Stick.B) == 0) then
        kidY = kidY + (remH / 2)
      elseif ((stick & Stick.T) == 0 and (stick & Stick.B) ~= 0) then
        kidY = kidY + remH
      end

      kid.props.x, kid.props.y, kid.props.w, kid.props.h = kidX, kidY, finalW, finalH
      currentX = currentX + cellW
    end
    currentY = currentY + rowHeights[row]
  end
end

return Layout
