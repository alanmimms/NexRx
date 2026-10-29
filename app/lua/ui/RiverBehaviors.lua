--[[
  RiverBehaviors.lua - Tag-parametric strategy tables for The River

  Encodes customizable interaction strategies (jump scrolling,
  playback cursor rendering, loupe activation) as first-class Lua tables.
]]

local RiverBehaviors = {}

-- Viewport Jump Scroll Strategy
RiverBehaviors.jumpScroll = {
  -- Calculate new timeOffset when playback reaches CAD break line
  calculateJumpTarget = function(currentOffset, visibleSpan, jumpRatio)
    jumpRatio = jumpRatio or 0.5
    local newOffset = currentOffset - (visibleSpan * jumpRatio)
    if newOffset < 0 then newOffset = 0 end
    return newOffset
  end,
  slideDurationMs = 150,
}

-- Playback Trace Rendering Strategy
RiverBehaviors.travelingScoped = {
  renderPlaybackTrace = function(river, data, count, x, y, w, h, fMinNorm, fMaxNorm)
    if river and river.renderLiveTrace then
      river.renderLiveTrace(data, x, y, w, h, fMinNorm, fMaxNorm)
    end
  end,
}

-- Bubble Zoom Loupe Strategy
RiverBehaviors.bubbleLoupe = {
  shouldActivateBubble = function(cursorX, cursorY, tracks, isAltActive)
    if not isAltActive then return false end
    if not tracks or #tracks == 0 then return false end
    local threshold = 30
    for _, track in ipairs(tracks) do
      if math.abs((track.y or 0) - cursorY) <= threshold then
        return true
      end
    end
    return false
  end,
  radius = 60,
  magnificationY = 2.5,
  magnificationX = 1.5,

  -- Anamorphic forward transform: (unmagnified -> magnified screen coordinate)
  transformForward = function(x, y, cx, cy, magX, magY)
    magX = magX or 1.5
    magY = magY or 2.5
    return cx + (x - cx) * magX, cy + (y - cy) * magY
  end,

  -- Anamorphic inverse transform: (magnified screen coordinate -> unmagnified canvas coordinate)
  transformInverse = function(screenX, screenY, cx, cy, magX, magY)
    magX = magX or 1.5
    magY = magY or 2.5
    return cx + (screenX - cx) / magX, cy + (screenY - cy) / magY
  end,
}

return RiverBehaviors
