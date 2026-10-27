--[[
  DecoderTrack.lua - Inline decoded text and activity ribbons for The River

  Implements adaptive Level of Detail (LOD) and tag-driven styling:
  - Colors are driven entirely by SetBox tags (e.g. mode.CW, mode.FT8, award.neededDX)
  - When zoomed in: renders decoded text inline over the signal's horizontal track.
  - When zoomed out: steps down to compact, color-coded activity ribbons.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")

local DecoderTrack = Widget.mkType("DecoderTrack", Widget)

-- Base defaults for all DecoderTracks
setbox.rule {
  id = "decodertrack-base-defaults",
  tags = { "widget.DecoderTrack" },
  priority = -1000,
  apply = {
    ribbonColor = "#22c55e",
    textColor = "#f8fafc",
    badgeBackground = "#050811",
    ribbonWidth = 2.0,
    ribbonOpacity = 0.55,
    textOpacity = 0.95,
  }
}

local function toRgba(val, defaultAlpha)
  if not val then return nil end
  if type(val) == "table" then
    local r = val[1] or val.r or 1
    local g = val[2] or val.g or 1
    local b = val[3] or val.b or 1
    local a = val[4] or val.a or defaultAlpha or 1
    return { r, g, b, a }
  elseif type(val) == "string" then
    local r, g, b, a = state.hexToRgb(val)
    return { r, g, b, a or defaultAlpha or 1 }
  end
  return nil
end

function DecoderTrack:init(def)
  def = def or {}
  self.freq = def.freq or 14.074e6
  self.mode = def.mode or "FT8"
  self.callsign = def.callsign or "W1AW"
  self.text = def.text or "CQ W1AW FN31"
  self.timeStart = def.timeStart or 0.0
  self.timeDuration = def.timeDuration or 15.0

  -- Ensure mode tag and any custom tags are attached to local widget context
  local initialTags = {}
  if def.tags then
    for _, t in ipairs(def.tags) do
      table.insert(initialTags, t)
    end
  end
  table.insert(initialTags, "mode." .. tostring(self.mode):upper())
  def.tags = initialTags

  Widget.init(self, def)
end

function DecoderTrack:getRibbonColor()
  local col = self.props.ribbonColor or self.props.color
  local alpha = self.props.ribbonOpacity or 0.55
  return toRgba(col, alpha) or { 0.2, 0.6, 1.0, alpha }
end
DecoderTrack.getModeColor = DecoderTrack.getRibbonColor

function DecoderTrack:getTextColor()
  local col = self.props.textColor or self.props.foreground
  local alpha = self.props.textOpacity or 0.95
  return toRgba(col, alpha) or { 1.0, 1.0, 1.0, alpha }
end

function DecoderTrack:renderInline(yPx, wPx, timeSpanSec, hzPerPx, loupeActive, loupeCx, loupeCy)
  local ribbonCol = self:getRibbonColor()
  local textCol = self:getTextColor()
  local ribbonWidth = self.props.ribbonWidth or 2.0

  -- Activity ribbon line
  System.drawLine(0, yPx, wPx, yPx, ribbonWidth, ribbonCol)

  -- Adaptive Level of Detail (LOD):
  -- If zoomed in (< 50 Hz/px) or inside magnifying loupe, render text inline
  local showText = (hzPerPx < 50)
  if not showText and loupeActive and loupeCy then
    if math.abs(yPx - loupeCy) < 40 then
      showText = true
    end
  end

  if showText then
    local label = string.format("%s: %s", tostring(self.callsign), tostring(self.text))
    local tw = System.measureText(label, 12) or (#label * 8)
    local tx = math.max(12, wPx - tw - 30)

    -- Background badge behind text for legibility over spectrum
    local badgeCol = toRgba(self.props.badgeBackground or "#050811", 0.88)
    System.drawRoundedRect(tx - 3, yPx - 8, tw + 6, 16, 3, badgeCol)
    System.drawText(label, tx, yPx - 6, 12, textCol)
  end
end

return DecoderTrack
