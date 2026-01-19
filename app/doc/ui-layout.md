# NexRx UI Layout System

The layout system provides automatic positioning and sizing for widgets in an immediate-mode GUI. It supports dock-based layouts, splits, horizontal/vertical stacking, and integrates with the event dispatch system for proper widget hierarchy.

## 1. Overview

### Philosophy

The layout system follows the immediate-mode GUI pattern: layout is calculated fresh every frame, not stored in a retained tree. This means:

- Layout regions are ephemeral - rebuilt each `draw()` call
- State is minimal - just a stack of regions and cursors
- Integration is simple - call layout functions, get positions back

### Architecture

```
+------------------+     +------------------+     +------------------+
|    layout.lua    | --> |   widgets.lua    | --> |   events.lua     |
|  (positioning)   |     |   (rendering)    |     |  (dispatch)      |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
   Region Stack            Widget Drawing           Event Bubbling
   Cursor Position         Hit Registration         Handler Lookup
```

### Basic Usage

```lua
local layout = require("ui.layout")

function draw()
    layout.begin(0, 0, windowW, windowH)

    -- Dock a 40-pixel header at top
    layout.dock("top", 40)
    -- draw header widgets here
    layout.endDock()

    -- Dock a 200-pixel sidebar on left
    layout.dock("left", 200)
    -- draw sidebar widgets here
    layout.endDock()

    -- Remaining area is the center content
    local x, y, w, h = layout.getRect()
    -- draw main content here

    layout.finish()
end
```

## 2. Dock Layouts

Dock layouts reserve space along an edge of the current region. The remaining space shrinks accordingly.

### Function Signature

```lua
layout.dock(side, size, name)
layout.endDock()
```

- `side`: "top", "bottom", "left", "right"
- `size`: pixels to reserve (height for top/bottom, width for left/right)
- `name`: optional string for debugging/events

### How Docking Works

```
Before dock("top", 40):          After dock("top", 40):
+------------------------+        +------------------------+
|                        |        |    DOCKED (40px)       |
|                        |        +------------------------+
|     CURRENT REGION     |  -->   |                        |
|                        |        |   REMAINING REGION     |
|                        |        |                        |
+------------------------+        +------------------------+
```

### Dock Order Matters

The order of dock calls affects the final layout:

```lua
-- Order 1: Top first, then left
layout.dock("top", 40)     -- Full width header
layout.endDock()
layout.dock("left", 200)   -- Left sidebar below header
layout.endDock()

-- Order 2: Left first, then top
layout.dock("left", 200)   -- Full height sidebar
layout.endDock()
layout.dock("top", 40)     -- Header to right of sidebar
layout.endDock()
```

### Nesting Docks

Docks can be nested within other docks:

```lua
layout.dock("left", 300)
    -- This creates a 300px wide region
    layout.dock("top", 50)
        -- Header within sidebar
    layout.endDock()
    layout.dock("bottom", 30)
        -- Footer within sidebar
    layout.endDock()
    -- Middle area of sidebar
layout.endDock()
```

### Edge Cases

- **Zero-size dock**: Creates an empty region, valid but useless
- **Oversized dock**: Region will have negative remaining space - avoid this
- **Missing endDock()**: Region stack becomes unbalanced, causes layout bugs

## 3. Split Layouts

Splits divide the current region proportionally into two parts.

### Horizontal Split

```lua
layout.splitH(ratio, name)  -- Start left side
-- draw left content
layout.nextSplit()          -- Move to right side
-- draw right content
layout.endSplit()           -- Finish split
```

### Vertical Split

```lua
layout.splitV(ratio, name)  -- Start top side
-- draw top content
layout.nextSplit()          -- Move to bottom side
-- draw bottom content
layout.endSplit()           -- Finish split
```

### Ratio Values

- `ratio`: 0.0 to 1.0, portion for first side
- Example: `splitH(0.25)` gives 25% left, 75% right
- Example: `splitV(0.5)` gives equal top/bottom

### Split Diagram

```
splitH(0.3):
+--------+------------------+
|  30%   |       70%        |
| (left) |     (right)      |
+--------+------------------+

splitV(0.4):
+--------------------------+
|           40%            |
|          (top)           |
+--------------------------+
|           60%            |
|        (bottom)          |
+--------------------------+
```

## 4. Stacking Layouts

Stacking layouts automatically position items sequentially.

### Horizontal Stacking

```lua
layout.beginHorizontal(spacing, name)

-- Items flow left-to-right
local x, y = layout.reserveSpace(100, 32)  -- First item
-- draw widget at x, y with size 100x32

x, y = layout.reserveSpace(80, 32)  -- Second item
-- draw widget at x, y with size 80x32

layout.endHorizontal()
```

### Vertical Stacking

```lua
layout.beginVertical(spacing, name)

-- Items flow top-to-bottom
local x, y = layout.reserveSpace(200, 28)  -- First item
x, y = layout.reserveSpace(200, 28)        -- Second item

layout.endVertical()
```

### Spacing

- `spacing`: pixels between items (default: 4)
- Applied automatically after each `reserveSpace()` call

### reserveSpace() Return Values

```lua
local x, y = layout.reserveSpace(width, height)
```

Returns the top-left corner where the widget should be drawn. The cursor advances automatically.

## 5. Coordinate System

### Origin and Axes

```
(0,0) -------- X+ -------->
  |
  |
  Y+
  |
  |
  v
```

- Origin: top-left corner of window
- X: increases rightward (pixels)
- Y: increases downward (pixels)
- All values are in pixels, not normalized

### Region Coordinates

Regions track:
- `x, y`: top-left corner of the region
- `w, h`: width and height of the region
- `cursorX, cursorY`: current drawing position within region

## 6. Cursor Model

The cursor tracks where the next item should be placed within a region.

### Getting Cursor Position

```lua
local x, y = layout.getCursor()
```

### Setting Cursor Position

```lua
layout.setCursor(relX, relY)  -- Relative to region origin
```

### Moving to Next Line

```lua
layout.newLine(height)  -- Move to start of next row
```

If `height` is nil, uses the maximum height seen in the current row.

### Cursor Flow in Horizontal Stacking

```
+-----------------------------------------------+
| [Item1] spacing [Item2] spacing [Item3]       |
|    ^                                          |
|    cursor starts here, moves right            |
+-----------------------------------------------+
```

### Cursor Flow in Vertical Stacking

```
+----------------+
| [Item1]        |
|   spacing      |
| [Item2]        |
|   spacing      |
| [Item3]        |
|    ^           |
|    cursor      |
+----------------+
```

## 7. Padding

Padding shrinks the usable area of a region.

### Applying Padding

```lua
layout.pad(pixels)  -- Shrink all sides by pixels
```

### Effect on Region

```
Before pad(12):                  After pad(12):
+------------------------+       +------------------------+
|                        |       |  12px padding          |
|                        |       |  +------------------+  |
|    FULL REGION         |  -->  |  |                  |  |
|                        |       |  | USABLE REGION    |  |
|                        |       |  |                  |  |
|                        |       |  +------------------+  |
+------------------------+       +------------------------+
```

### Padding affects:
- Region bounds (x, y, w, h)
- Cursor position (reset to new top-left)
- Does NOT affect parent regions

## 8. Widget Integration

Widgets use the layout API to get their positions.

### Manual Positioning

```lua
local x, y, w, h = layout.getRect()
ui.button("btn", "Click", x + 10, y + 10, 100, 32)
```

### Automatic Positioning with Stacking

```lua
layout.beginHorizontal()
local x, y = layout.reserveSpace(100, 32)
ui.button("btn1", "First", x, y, 100, 32)
x, y = layout.reserveSpace(100, 32)
ui.button("btn2", "Second", x, y, 100, 32)
layout.endHorizontal()
```

### Layout-Aware Widget Pattern

Widgets can be designed to use the layout system:

```lua
function ui.layoutButton(id, label, w, h, tags)
    local x, y = layout.reserveSpace(w, h)
    return ui.button(id, label, x, y, w, h, tags)
end
```

## 9. Event Hierarchy

The layout system tracks region hierarchy for event dispatch. Widgets inherit their parent region from the current layout context.

### How Hierarchy Works

```lua
layout.dock("left", 200)       -- Creates region "dock_left"
    ui.button("btn1", ...)     -- Parent = "dock_left"

    layout.beginVertical()     -- Creates region "vstack"
        ui.slider("sld1", ...) -- Parent = "vstack"
        ui.checkbox("chk", ..) -- Parent = "vstack"
    layout.endVertical()

layout.endDock()
```

### Event Bubbling

Events bubble from widget to parent regions until handled:

```
User clicks checkbox:
  1. Check "chk" handlers -> not handled
  2. Check "vstack" handlers -> not handled
  3. Check "dock_left" handlers -> not handled
  4. Check "root" handlers -> not handled
  5. Unhandled event logged
```

### Wiring Events to Layout

```lua
-- In init():
events.init()
layout.setEventsModule(events)
ui.setEventsModule(events)
ui.setLayoutModule(layout)
```

## 10. API Reference

### Initialization

| Function | Description |
|----------|-------------|
| `layout.begin(x, y, w, h)` | Start layout for a frame with root region |
| `layout.finish()` | End layout for a frame, cleanup |

### Region Query

| Function | Returns | Description |
|----------|---------|-------------|
| `layout.getRect()` | `x, y, w, h` | Current region bounds |
| `layout.getCursor()` | `x, y` | Current cursor position |
| `layout.getRemainingSize()` | `w, h` | Space remaining in region |
| `layout.getDepth()` | `number` | Region stack depth |

### Dock Operations

| Function | Description |
|----------|-------------|
| `layout.dock(side, size, name)` | Dock region to side ("top"/"bottom"/"left"/"right") |
| `layout.endDock()` | End current dock, return to parent |

### Split Operations

| Function | Description |
|----------|-------------|
| `layout.splitH(ratio, name)` | Split horizontally, enter left side |
| `layout.splitV(ratio, name)` | Split vertically, enter top side |
| `layout.nextSplit()` | Move to other side of split |
| `layout.endSplit()` | End split, return to parent |

### Stacking Operations

| Function | Description |
|----------|-------------|
| `layout.beginHorizontal(spacing, name)` | Begin left-to-right stacking |
| `layout.endHorizontal()` | End horizontal stacking |
| `layout.beginVertical(spacing, name)` | Begin top-to-bottom stacking |
| `layout.endVertical()` | End vertical stacking |
| `layout.reserveSpace(w, h)` | Reserve space, return position |

### Cursor Operations

| Function | Description |
|----------|-------------|
| `layout.setCursor(x, y)` | Set cursor position (relative to region) |
| `layout.newLine(height)` | Move to next row |
| `layout.space(amount)` | Add spacing in layout direction |
| `layout.indent(amount)` | Indent cursor right |
| `layout.unindent(amount)` | Unindent cursor left |

### Padding and Alignment

| Function | Returns | Description |
|----------|---------|-------------|
| `layout.pad(pixels)` | - | Shrink region by padding |
| `layout.center(w, h)` | `x, y` | Get position to center item |
| `layout.alignRight(w)` | `x` | Get x to right-align item |
| `layout.alignBottom(h)` | `y` | Get y to bottom-align item |

### Event Integration

| Function | Description |
|----------|-------------|
| `layout.setEventsModule(events)` | Connect to events module |
| `layout.getCurrentRegionId()` | Get current region ID for events |
| `layout.getCurrentRegionName()` | Get current region name |

### Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `layout.defaultPadding` | 8 | Default padding pixels |
| `layout.defaultSpacing` | 4 | Default spacing between items |

## Examples

### Complete Sidebar Example

```lua
layout.dock("left", 260)
do
    local x, y, w, h = layout.getRect()
    ui.panel(x, y, w, h)
    layout.pad(12)

    ui.label(layout.getCursor(), "Settings", {"Title"})
    layout.newLine(24)

    layout.beginVertical(8)

    local cx, cy = layout.reserveSpace(w - 24, 24)
    enabled = ui.checkbox("enable", "Enable Feature", cx, cy, enabled)

    cx, cy = layout.reserveSpace(w - 24, 24)
    value = ui.slider("value", cx, cy, w - 24, 0, 100, value)

    layout.endVertical()
end
layout.endDock()
```

### Toolbar with Horizontal Layout

```lua
layout.dock("top", 40)
do
    layout.pad(4)
    layout.beginHorizontal(4)

    local bx, by = layout.reserveSpace(80, 32)
    if ui.button("new", "New", bx, by, 80, 32) then
        -- handle new
    end

    bx, by = layout.reserveSpace(80, 32)
    if ui.button("open", "Open", bx, by, 80, 32) then
        -- handle open
    end

    bx, by = layout.reserveSpace(80, 32)
    if ui.button("save", "Save", bx, by, 80, 32) then
        -- handle save
    end

    layout.endHorizontal()
end
layout.endDock()
```

### Split View

```lua
layout.splitH(0.3)
do
    -- Left panel (30%)
    local x, y, w, h = layout.getRect()
    ui.panel(x, y, w, h)
    -- draw file tree
end
layout.nextSplit()
do
    -- Right panel (70%)
    local x, y, w, h = layout.getRect()
    ui.panel(x, y, w, h)
    -- draw editor
end
layout.endSplit()
```
