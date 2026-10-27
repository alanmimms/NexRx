--[[
  LiveTrace.lua - Right Gutter widget containing S-Meter and vertical Live Trace

  Renders the compact dual-decay horizontal S-Meter at the top,
  and the vertical Tektronix P31 phosphor spectral amplitude curve below it.
]]

local setbox = require("SetBox")
local state = require("ui.State")
local Widget = require("ui.Widget")
local SMeter = require("ui.SMeter")

local LiveTrace = Widget.mkType("LiveTrace", Widget.Container)

setbox.rule {
  id = "live-trace-defaults",
  tags = { "widget.LiveTrace" },
  priority = -1000,
  apply = {
    prefW = 90,
    background = "#070c16",
    border = "#131f33",
    gridColor = { 0.15, 0.25, 0.35, 0.4 },
    nowLineColor = { 0.15, 0.95, 0.35, 0.9 },
  }
}

function LiveTrace:init(def)
  def = def or {}
  self.smeter = SMeter({ id = (def.id or "livetrace") .. "-smeter", prefH = 34 })
  def.kids = { self.smeter }
  Widget.Container.init(self, def)
end

function LiveTrace:calcMetrics()
  if (self.props.prefW or 0) == 0 then self.props.prefW = 90 end
  if (self.props.prefH or 0) == 0 then self.props.prefH = 300 end
  if self.smeter then self.smeter:calcMetrics() end
end

function LiveTrace:layout(x, y, w, h)
  self.props.x, self.props.y, self.props.w, self.props.h = x, y, w, h
  local meterH = 36
  if self.smeter then
    self.smeter:layout(0, 0, w, meterH)
  end
end

function LiveTrace:drawSelf()
  local w = self.props.w or 0
  local h = self.props.h or 0
  if w <= 0 or h <= 0 then return end

  local meterH = 36
  local traceY = meterH
  local traceH = math.max(10, h - traceY)

  -- Background
  local bgR, bgG, bgB = state.hexToRgb(self.props.background or "#070c16")
  System.drawRect(0, 0, w, h, { bgR, bgG, bgB, 0.95 })

  -- Green vertical NOW line on left edge
  local nowColor = self.props.nowLineColor or { 0.15, 0.95, 0.35, 0.9 }
  System.drawLine(0, 0, 0, h, 2.0, nowColor)

  -- Tiny 'NOW' tag at the top
  System.drawText("NOW", 4, 2, 10, nowColor)

  -- Grid lines across trace area
  local gridColor = self.props.gridColor or { 0.15, 0.25, 0.35, 0.4 }
  for i = 1, 3 do
    local gx = w * (i / 4.0)
    System.drawLine(gx, traceY, gx, traceY + traceH, 1.0, gridColor)
  end

  -- Render vertical phosphor trace via RiverRenderer
  local riverEngine = _G.river or _G.waterfall
  local data = _G.lastSpectrumData
  if riverEngine and riverEngine.renderLiveTrace and data and #data > 0 then
    local ox, oy = state.getOffset()
    riverEngine.renderLiveTrace(data, ox, oy + traceY, w, traceH,
                                self.props.freqMinNorm or 0.0,
                                self.props.freqMaxNorm or 1.0)
  end
end

return LiveTrace
