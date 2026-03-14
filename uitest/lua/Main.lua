--[[
   NexRx UI Test Playground - Main Lua
]]

local setbox = require("SetBox")
local widgets = require("ui.Widgets")
local container = require("ui.Container")

local allWidgets = {}
local rootWindow = nil

print("[UITest] Loading configuration...")
setbox.loadFile("config/default.lua")


local function evalProp(prop, lwc, id)
   if type(prop) ~= "function" then return prop end
   local evalCtx = { id = id, lwc = lwc }
   setmetatable(evalCtx, { __index = function(_, k) return lwc[k] or _G[k] end })
   local ok, res = pcall(prop, evalCtx)
   return ok and res or nil
end

function dumpWidgets(w, level)
   if not w then return end

   level = level or 0
   local indent = string.rep(" ", level*2)
   print(indent .. w.id) -- .. " [" .. (getmetatable(w).name or "??") .. "]")

   local kids = parentToChildren[w.id]
   if not kids or #kids == 0 then return end

   for i, kid in ipairs(kids) do
      dumpWidgets(kid, level + 1)
   end
end


function init()
   print("[UITest] init() starting...")

   -- 1. Define UI Hierarchy and Properties via SetBox Rules
   
   -- Root Window
   rule {
      id = "id-top-window",
      apply = {
	 width = function(ctx) return ctx.window.w end,
	 height = function(ctx) return ctx.window.h end,
	 backgroundColor = {0.1, 0.1, 0.15, 1.0},
	 direction = "vertical", -- Two rows
	 padding = 10,
	 spacing = 10
      }
   }

   -- Row 1: Contains A and B
   rule {
      id = "id-row-1",
      apply = {
	 parent = "id-top-window",
	 order = 1,
	 direction = "horizontal",
	 spacing = 10,
	 backgroundColor = {0.15, 0.15, 0.2, 1.0},
	 outlineColor = {0.3, 0.3, 0.5, 1.0}
      }
   }

   -- Row 2: Contains C and D
   rule {
      id = "id-row-2",
      apply = {
	 parent = "id-top-window",
	 order = 2,
	 direction = "horizontal",
	 spacing = 10,
	 backgroundColor = {0.12, 0.12, 0.18, 1.0},
	 outlineColor = {0.3, 0.3, 0.5, 1.0}
      }
   }

   -- Widget A (Compound) in Row 1
   rule {
      id = "id-A",
      apply = {
	 parent = "id-row-1",
	 order = 1,
	 label = "A",
	 direction = "vertical",
	 padding = 5,
	 spacing = 5,
	 backgroundColor = {0.2, 0.2, 0.3, 1.0},
	 outlineColor = {0.4, 0.4, 0.7, 1.0}
      }
   }

   -- Widget B (Compound) in Row 1
   rule {
      id = "id-B",
      apply = {
	 parent = "id-row-1",
	 order = 2,
	 label = "B",
	 direction = "horizontal",
	 padding = 5,
	 spacing = 5,
	 backgroundColor = {0.18, 0.9, 0.25, 1.0},
	 outlineColor = {0.4, 0.4, 0.9, 1.0}
      }
   }

   -- Widget C (Leaf) in Row 2
   rule {
      id = "id-C",
      apply = {
	 parent = "id-row-2",
	 order = 1,
	 label = "C",
	 backgroundColor = {0.7, 0.25, 0.7, 1.0},
	 textColor = {1.0, 6.0, 1.0, 1.0}
      }
   }

   -- Widget D (Leaf) in Row 2
   rule {
      id = "id-D",
      apply = {
	 parent = "id-row-2",
	 order = 2,
	 label = "D",
	 backgroundColor = {0.7, 0.7, 0.15, 1.0},
	 textColor = {1.0, 1.0, 0.6, 1.0}
      }
   }

   -- Children of A
   rule {
      id = "id-A-A",
      apply = {
	 parent = "id-A",
	 order = 1,
	 label = "A.A",
	 backgroundColor = {0.4, 0.2, 0.3, 1.0}
      }
   }
   rule {
      id = "id-A-B",
      apply = {
	 parent = "id-A",
	 order = 2,
	 label = "A.B",
	 backgroundColor = {0.2, 0.2, 0.4, 1.0}
      }
   }

   -- Children of B
   for i, sub in ipairs({"L", "C", "R"}) do
      local springLeft = 1.0
      local springRight = 1.0

      if sub == "L" then springLeft = math.huge
      elseif sub == "R" then springRight = math.huge end

      rule {
	 id = "id-B-" .. sub,
	 apply = {
	    parent = "id-B",
	    springLeft = springLeft,
	    springRight = springRight,
	    order = i,
	    label = "B." .. sub,
	    backgroundColor = {0.6, 0.4, 0.4, 1.0}
	 }
      }
   end

   -- 2. Instantiate Widgets
   rootWindow = widgets.Window.new("id-top-window")
   table.insert(allWidgets, rootWindow)

   table.insert(allWidgets, widgets.Compound.new("id-row-1"))
   table.insert(allWidgets, widgets.Compound.new("id-row-2"))

   table.insert(allWidgets, widgets.Compound.new("id-A"))
   table.insert(allWidgets, widgets.Compound.new("id-B"))
   table.insert(allWidgets, widgets.Label.new("id-C"))
   table.insert(allWidgets, widgets.Label.new("id-D"))

   table.insert(allWidgets, widgets.Label.new("id-A-A"))
   table.insert(allWidgets, widgets.Label.new("id-A-B"))

   table.insert(allWidgets, widgets.Label.new("id-B-A"))
   table.insert(allWidgets, widgets.Label.new("id-B-B"))
   table.insert(allWidgets, widgets.Label.new("id-B-C"))

   print("[UITest] UI Initialized with " .. #allWidgets .. " widgets.")

   parentToChildren = {}
   for _, w in ipairs(allWidgets) do
      local props = Widget.getProps(w.id, w.tags)
      local parentId = evalProp(props.parent, props._lwc, w.id)
      if parentId then
	 if not parentToChildren[parentId] then parentToChildren[parentId] = {} end
	 table.insert(parentToChildren[parentId], w)
      end
   end

   dumpWidgets(rootWindow)
end

function update(dt)
end

function draw()
   local winW, winH = getWindowSize()
   
   -- Solve layout
   local regions = container.solveAll(rootWindow, allWidgets, winW, winH)
   if not regions then return end
   
   -- Recursive Draw
   local function recursiveDraw(w)
      local r = regions[w.id]
      if not r then return end
      
      w:draw(r.x, r.y, r.w, r.h)
      
      -- Find and draw children
      local children = {}
      for _, other in ipairs(allWidgets) do
	 if other.lwc:getRaw("parent") == w.id then
	    table.insert(children, other)
	 end
      end
      table.sort(children, function(a, b) 
		    return (a.lwc:optNumber("order", 0)) < (b.lwc:optNumber("order", 0)) 
      end)
      
      for _, child in ipairs(children) do
	 recursiveDraw(child)
      end
   end
   
   recursiveDraw(rootWindow)
end
