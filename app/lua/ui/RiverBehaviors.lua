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
  shouldActivateBubble = function(cursorX, cursorY, tracks, zoomFactor)
    if not tracks or #tracks == 0 then return false end
    -- Activate if multiple tracks collide within threshold
    local collisionThresholdPx = 14
    local nearbyCount = 0
    for _, track in ipairs(tracks) do
      if math.abs((track.y or 0) - cursorY) < collisionThresholdPx then
        nearbyCount = nearbyCount + 1
      end
    end
    return nearbyCount >= 2
  end,
  radius = 60,
  magnification = 2.0,
}

return RiverBehaviors
