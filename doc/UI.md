# NexRx UI Architecture Guide

## Overview

NexRx is a high-frequency (HF) amateur radio receiver built on a novel
hardware design. It streams six channels of I/Q samples at 96kS/s (or
higher) via a 480Mb/s USB connection to a host PC.

The software architecture divides responsibilities strictly to
maintain high performance while offering maximum flexibility. The host
application is built in C++ to handle digital signal processing (DSP)
and real-time data constraints. The user interface, however, is driven
entirely by Lua 5.4, utilizing Raylib and SDL2 to render an
immediate-mode, OpenGL-accelerated GUI that operates similarly to a
high-framerate video game UI.

This document outlines the core concepts, data structures, and layout
philosophies of the NexRx UI engine.

---

## Core Philosophy

* **Strict Language Separation:** C++ does the heavy lifting (DSP,
  hardware interfacing, raw OpenGL rendering). Lua dictates the logic,
  layout, state, and UI hierarchy.
* **Data-Driven UI:** The UI is constructed as a tree of Lua tables.
  This structural approach ensures the UI can easily be serialized,
  modified at runtime, and eventually manipulated via a Drag-and-Drop
  (DND) visual editor.
* **Decoupled Layouts:** Widgets do not contain their own positioning
  math. They declare their constraints (stuck edges, proportional
  mapping, flex), and a contextual layout function processes the
  container to position the children.
* **FPS-Style Rendering:** The UI does not use a "dirty rectangle" or
  traditional retained redraw system. It renders continuously,
  allowing for seamless zooming, panning, and animations in critical
  components like the RF spectrum and waterfall.

---

## Architecture Components

### 1. The Widget Types and Instances

The UI differentiates strictly between a **Widget Type** (the
definition or class) and a **Widget Instance** (the realized object in
the tree).

A Lua metatable DSL (Domain Specific Language) allows developers and
users to construct the UI declaratively. Every hierarchy begins with a
`Window` root node.

```lua
local WidgetInstance = {}
WidgetInstance.__index = WidgetInstance

-- Defines a new widget class
local function defineWidgetType(typeName)
  local widgetType = { typeName = typeName }
  
  setmetatable(widgetType, {
    __call = function (cls, def)
      local instance = {
        type = cls.typeName,
        id = def.id or "",
        props = def.props or {},
        tags = def.tags or {},
        children = def.children or {},
        parent = nil
      }
      
      local nChildren = #instance.children
      for i = 1, nChildren do
        instance.children[i].parent = instance
      end
      
      setmetatable(instance, WidgetInstance)
      return instance
    end
  })
  
  return widgetType
end

local Window = defineWidgetType("Window")
local VBox = defineWidgetType("VBox")
local Checkbox = defineWidgetType("Checkbox")

```

### 2. Theming and State (Tags)

State is managed through a hierarchical tag system. Themes are defined
globally using normalized RGBA color values (0.0 to 1.0). When a
widget renders, it resolves its style by traversing its tag hierarchy
(Local -> Container -> Window -> Global).

```lua
local function hexToRgba(hexStr)
  local hex = string.gsub(hexStr, "#", "")
  local hexLen = string.len(hex)
  
  local r = tonumber(string.sub(hex, 1, 2), 16) / 255.0
  local g = tonumber(string.sub(hex, 3, 4), 16) / 255.0
  local b = tonumber(string.sub(hex, 5, 6), 16) / 255.0
  local a = 1.0
  
  if (hexLen == 8) then
    a = tonumber(string.sub(hex, 7, 8), 16) / 255.0
  end
  
  return { r, g, b, a }
end

local theme = {
  default = {
    background = { style = "fill", color = hexToRgba("#1c1c1cFF") },
    foreground = { textColor = { 0.86, 0.86, 0.86, 1.0 }, lineColor = { 0.0, 1.0, 0.0, 1.0 } }
  }
}

```

This allows a global state change (e.g., adding a `"transmitting"`
tag) to instantly alter the appearance or behavior of deeply nested
leaf widgets, such as checkboxes or sliders, without requiring tight
coupling.

### 3. Layout Strategies

Layout logic is treated as a swappable strategy applied to containers.
The layout function interprets a child's constraints (e.g., edge
sticking) and distributes available space.


The layout process uses the Stick bits to pin the edges of a given
widget to its siblings corresponding adjacent or parent's
corresponding edge to make its size fill or not fill the available
space. This is done in box X and Y axes based in the stickiness bits.
The size of a label is taken into account for its height and width to
force the parent's dimensions to encompass it if it's tallest or
widest of the kids.


#### Generic Sticking and Stretching

For standard UI elements (buttons, panels), widgets declare their
adhesion to container boundaries via the `props.stick` table.

```lua
local function applyHorizontalSticking(container)
  local nChildren = #container.children
  local parentWidth = container.props.width
  local currentX = container.props.x
  
  for i = 1, nChildren do
    local child = container.children[i]
    local stick = child.props.stick or {}
    local padLeft = child.props.padLeft or 0
    local padRight = child.props.padRight or 0
    
    if (stick.left and stick.right) then
      child.props.width = parentWidth - padLeft - padRight
      child.props.x = currentX + padLeft
    elseif (stick.left) then
      child.props.x = currentX + padLeft
    elseif (stick.right) then
      child.props.x = (currentX + parentWidth) - child.props.width - padRight
    end
    
    currentX = currentX + child.props.width + padLeft + padRight
  end
end

```

#### Proportional Mapping (Data-Driven Placement)

For SDR-specific widgets, like a `SignalBox` representing a
demodulation target on the spectrum, positioning is mapped directly
from the domain data (frequency) to screen space, bypassing
traditional flex or grid rules.

```lua
local function spectrumLayout(container)
  local fMin = container.props.freqMin
  local fMax = container.props.freqMax
  local span = fMax - fMin
  local nChildren = #container.children
  
  for i = 1, nChildren do
    local child = container.children[i]
    if (child.type == "SignalBox") then
      local startFreq = child.props.centerFreq - (child.props.bandwidth / 2.0)
      local xRatio = (startFreq - fMin) / span
      local wRatio = child.props.bandwidth / span
      
      child.props.x = container.props.x + (xRatio * container.props.width)
      child.props.width = wRatio * container.props.width
    end
  end
end

```

### 4. The C++ Bridge

The C++ host provides primitive rendering functions and handles
high-throughput data. To maintain performance and follow project
conventions, the API utilizes standard C strings (`const char*`)
rather than standard library string objects, avoiding unnecessary
allocations during the render loop.

Functions are generalized, and templates are reserved strictly for
handling varying DSP data types.

```cpp
class RenderBridge {
public:
  void drawWidgetBorder(const char* widgetId, int x, int y, int w, int h, const char* borderStyle) {
    constexpr int borderThickness = 2; 
    
    if (strcmp(borderStyle, "dashed") == 0) {
      // Execute Raylib dashed line draw
    } else if (strcmp(borderStyle, "solid") == 0) {
      // Execute Raylib solid line draw
    }
  }

  template <typename DSP_PAYLOAD>
  void drawWaterfallRegion(const char* widgetId, int x, int y, int w, int h, const DSP_PAYLOAD* dspData, int dataSize) {
    constexpr int bytesPerPixel = 4;
    constexpr int textureWidth = 1024;
    constexpr int maxBufferSize = textureWidth * 1024 * bytesPerPixel; 
    
    for (int i = 0; i < dataSize; ++i) {
      // Map dspData[i] to waterfall texture and render via Raylib
    }
  }
};

```
