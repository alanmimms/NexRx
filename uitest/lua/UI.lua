local Widget = require("Widget")
local Color = require("Color")
local Stick = Widget.Stick

local uiTree = Widget.Window {
  id = "Window",
  children = {
    Widget.VerticalColumn {
      id = "W",
      borderColor = Color("#F00"),
      metrics = { flexW = 1, flexH = 1, stick = 0 },
      children = {
        Widget.Label { 
          id = "W.A", 
          props = { text = "W.A" }, 
          borderColor = Color("#0F0"),
          metrics = { flexW = 1, prefH = 40, stick = Stick.T }
        },
        Widget.HorizontalRow {
          id = "W.B",
          borderColor = Color("#00F"),
          metrics = { flexW = 1, flexH = 1, stick = Stick.B },
          children = {
            Widget.Label { 
              id = "B.L", props = { text = "B.L" }, borderColor = Color("#FF0"),
              metrics = { flexW = 1, flexH = 1, stick = Stick.L }
            },
            Widget.Label { 
              id = "B.C", props = { text = "B.C" }, borderColor = Color("#F0F"),
              metrics = { flexW = 1, flexH = 1, stick = Stick.mk("LR") }
            },
            Widget.Label { 
              id = "B.R", props = { text = "B.R" }, borderColor = Color("#0FF"),
              metrics = { flexW = 1, flexH = 1, stick = Stick.R }
            },
          }
        }
      }
    },
    Widget.VerticalColumn {
      id = "X",
      borderColor = Color("#FA0"),
      metrics = { flexW = 1, flexH = 1, stick = Stick.mk("TLBR") },
      children = {
        Widget.Label { 
          id = "X.F", props = { text = "X.F" }, borderColor = Color("#AF0"),
          metrics = { flexW = 1, flexH = 1, stick = Stick.mk("TLBR") }
        },
        Widget.Label { 
          id = "X.G", props = { text = "X.G" }, borderColor = Color("#0AF"),
          metrics = { flexW = 1, flexH = 1, stick = Stick.mk("TLBR") }
        },
      }
    }
  }
}

local function renderUI(bridge, width, height)
  uiTree:layout(0, 0, uiTree.props.w or width, uiTree.props.h or height)
  uiTree:draw(bridge)
end

local function onResize(w, h)
  if uiTree and uiTree.onResize then
    uiTree:onResize(w, h)
  end
end

return {
  render = renderUI,
  onResize = onResize
}
