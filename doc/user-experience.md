# NexRx: User Experience & Features
## Interface Design, Workflows, and Operating Features

### The Browser-Native Interface

Opening NexRx feels nothing like traditional radio software. There's
no installation process, no driver hunting, no compatibility concerns
across operating systems. Plug in the USB cable, open Chrome or
Safari, navigate to the captive portal, and you're immediately
presented with a modern, responsive interface that feels more like a
professional audio application than a ham radio.

The interface adapts to your screen size and usage patterns. On a
large monitor, you might have multiple waterfall displays showing
different zoom levels, constellation plots for digital modes, and
audio analysis tools. On a tablet, the interface reorganizes to
prioritize the most commonly used controls while keeping advanced
features accessible through intuitive gestures.

### Waterfall-Centric Operation

The waterfall display serves as far more than just a spectrum
visualization - it becomes a primary control surface. Mouse wheel
scrolling tunes frequency with pixel-level precision, while modifier
keys enable different functions directly on the graphical display.

**Zooming**: The spectrum and waterfall can be zoomed horizontally to
examine signals in high detail or to see the entire 96 kHz bandwidth
at once. Zooming is controlled by the `+` and `-` keys (defined via
SetBox rules). The system maintains a fractional zoom level, ensuring
that zooming out never exceeds the point where the spectrum fills the
available display width. Zooming specifically affects the frequency
(horizontal) axis and automatically updates the graticule legend and
all active SignalBoxes.

**SignalBoxes**: NexRx introduces the concept of **SignalBoxes**—visual
representations of active tuned signals. Unlike a traditional radio
with a single VFO, NexRx allows you to create multiple SignalBoxes
on the spectrum (using the `/` key).
- Each SignalBox tracks its own frequency, mode, and bandwidth.
- Clicking a SignalBox selects it, making it the "active" receiver
  controlled by the VFO and mode buttons.
- SignalBoxes can be dragged across the spectrum to retune.
- Ghost SignalBoxes (temporary previews) allow you to see where a 
  new box will be placed before committing.
- When zooming, SignalBoxes automatically recalculate their 
  visual size and position to remain locked to their tuned 
  frequencies.

```mermaid
graph TD
    A[Waterfall Display] --> B[Primary Tuning]
    A --> C[Zoom Control]
    A --> D[Filter Width]
    A --> E[Signal Analysis]
    A --> J[SignalBoxes]
    
    B --> F[Mouse Wheel: Frequency]
    C --> G[+/- Keys: Zoom Level]
    D --> H[Shift+Drag: Passband]
    E --> I[Click: Quick Tune]
    J --> K[/ Key: Add SignalBox]
    
    style A fill:#e3f2fd
    style F fill:#e8f5e8
    style G fill:#e8f5e8
    style H fill:#e8f5e8
    style I fill:#e8f5e8
    style K fill:#e8f5e8
```

### The SetBox Generalization Paradigm

NexRx is powered by **SetBox**, a rule-based configuration and styling
system that treats every aspect of the interface—from the color of a
button to the logic of a frequency readout—as a resolvable property.
The power of this mechanism comes from its **consistent
applicability** and **strict rule-driven nature**.

#### Hierarchical Spring Layout
NexRx uses a recursive spring-based layout system to manage complex
UI hierarchies like the sidebar.

**Incompressible Minimums**: Every UI element (labels, checkboxes,
groups) calculates its own "floor" size based on its content, where
leaf widgets have a fixed size based on their implementation and
current rule resolution. The layout engine is prohibited from
shrinking any element below this limit. This guarantees that even on
small screens, the UI remains legible and interactive, with overflow
items simply extending beyond the visible area rather than being
crushed.

**Edge Magnetism (Stickiness)**: Widgets define "magnetic" properties 
to determine how they respond as their parent container (the window or 
a parent group) grows:
- **Attachment**: Sticking a single edge (e.g., `stickRight`) keeps a 
  component locked to that boundary as the parent area expands.
- **Stretching**: Sticking opposing edges (e.g., `stickTop` AND 
  `stickBottom`) forces a component to expand to fill the available 
  dimension in its parent.
- **Hierarchical Magnetism**: This behavior is fractal. A Sidebar 
  sticks to the window's edges to claim its area, and then acts as a 
  magnetic parent for its own internal stack of groups.

**Proportional Expansion**: When a widget is stretched by magnetism, 
it uses **Spring Strength** (`springY` or `springX`) to determine how 
much of the extra space it should claim relative to its peers.
- Elements with `springY = 0` stay at their incompressible minimum.
- Elements with `springY > 0` expand proportionally.
- **Elastic Space**: By placing high-strength springs (Spacers) 
  between fixed-size components, the UI "breathes" naturally, 
  distributing gaps as the container grows.

**Fractal Resolution**: The layout is resolved recursively. The root 
window distributes space to high-level magnets (Sidebar, Center Area), 
which then act as parents for their own internal spring-loaded stacks.

#### 1. No Hardcoded Defaults
In NexRx, there are no "default values" buried in the source code. If
a widget needs to know its border width, its background color, or even
the text it should display, it must ask SetBox. If no rule matches the
current context, the system explicitly fails. This enforces a
discipline where every behavior and visual element is declared in
configuration files, making the entire application a "blank canvas"
for the user.

#### 2. Local Widget Context (LWC)
Every UI element is a standalone object (e.g., `Button.lua`,
`Slider.lua`) that operates within a **Local Widget Context**. When a
widget is drawn, it creates an LWC that combines:
*   **Specific Identity**: A unique ID tag (e.g., `id.vfo-slider`).
*   **Generic Type**: A widget type tag (e.g., `widget.Slider`).
*   **Hierarchical Parentage**: Inherited tags from its parent
    container (e.g., `widget.Sidebar`).
*   **Global State**: Global tags like the current band (`20m`) or
    theme (`theme.Dark`).

#### 3. Rule Resolution: Specificity Over Priority
SetBox uses a sophisticated resolution engine where **specificity**
(the number of matching tags) takes precedence. This allows for
surgical overrides. For example:
*   A rule matching `{"widget.Button"}` sets the global button style.

*   A rule matching `{"widget.Sidebar", "widget.Button"}` overrides
    that style only for buttons inside sidebars.

*   A rule matching `{"id.rx-toggle", "state.Active"}` can change the
    text and color of one specific button only when it is engaged.

#### 4. Generalized Content
Even UI strings are generalized. A `Label` widget can resolve its
`text` property from a rule, or use a dynamic callback for real-time
data like frequency. For instance, the "ACTIVE TAGS" header is not a
hardcoded string; it is a property resolved by a `Label` widget
matching the `activeTags.title` tag. This makes the UI entirely
localizable and customizable without touching a single line of
procedural code.

#### 5. Tag-Driven Interaction & Focus
NexRx eschews traditional discrete "focus" state variables. Instead,
it employs a **dynamic focus-by-hover** model driven by the same
SetBox tag resolution used for styling.

*   **Dynamic Context Propagation**: As the mouse moves across the
    interface, the system identifies the widget under the cursor and
    propagates its entire set of tags (type, ID, and groups) to the
    global active tag list.
*   **Implicit Focus Rules**: Interactive behaviors are defined by
    rules that intersect an input event with these propagated tags.
    For example, a rule matching `{"event.KeyDown-Digit",
    "widget.VFOControl"}` handles frequency entry. Because the
    Spectrum and Waterfall widgets also carry the `widget.VFOControl`
    tag, they implicitly "focus" the VFO when hovered without
    requiring any procedural state management.
*   **Reactive Highlighting**: Visual feedback follows the same logic.
    A frequency display highlights itself only when it detects that
    its group or identity tags are currently global (meaning its
    control area is hovered) or when specific state tags like
    `state.VFOEditing` are active.
*   **Event Gobbling by Priority**: The event dispatch system bubbles
    through the hierarchy, allowing specific rules to "gobble" events.
    This allows complex modes—like the frequency entry mode—to hijack
    keyboard input by providing high-priority handlers that only
    trigger when the correct environmental tags are present.

### Setbox Workflow in Practice

Imagine you're preparing for the ARRL DX Contest and want to set up
configurations for different bands and modes.

**Setting Up Base Configurations**: Start by creating a "Contest-Base"
setbox rule that defines common contest settings—reduced font sizes
for a compact UI, specific antenna selections, and your contest
callsign.

**Band-Specific Adjustments**: Create rules for `Contest-20m` and
`Contest-40m` that inherit from the base but add band-specific AGC
settings optimized for weak signal work versus pile-up busting.

**Live Operation**: Switching bands or modes instantly re-evaluates
the SetBox context. The UI instantly reflects new colors, labels, and
even layout adjustments because every widget's `:draw()` method
re-queries its LWC for its current properties.

### Advanced Visualization and Analysis

Because significant DSP processing happens in the browser, NexRx can
provide visualization and analysis tools that would be impossible in
traditional hardware radios.

**Constellation Diagrams**: For digital modes like PSK31 or FT8,
real-time constellation plots provide immediate feedback on signal
quality and decoding performance.

**Audio Analysis Tools**: Switchable oscilloscope and spectrum
analyzer views of demodulated audio help with precise adjustment of
receive and transmit audio equalization.

### Memory and Logging Systems

Traditional memory channels become much more powerful in the setbox
paradigm. Instead of just storing frequency and mode, a NexRx memory
can capture the complete operating state - antenna selections, DSP
settings, power levels, even waterfall color schemes.

**Tagged Memory System**: Memories include arbitrary tags, colors,
creation timestamps, and usage tracking. You might tag memories with
"DX", "Contest", "Ragchew", or "EmComm" and then filter displays to
show only relevant memories for your current activity.

**Complete State Recording**: The system maintains a chronological log
of all state changes with replay capability. This feature proves
invaluable for debugging equipment problems or analyzing propagation
changes.

### Development and Customization

The SetBox architecture makes customization and enhancement much more
accessible than traditional radio firmware modification.

The open-source nature means the entire ham community can contribute
improvements, from bug fixes and feature additions to completely new
interface paradigms that leverage the flexible setbox foundation.
Because widgets are standalone objects that rely strictly on rules,
adding a new UI component is as simple as defining a new class and its
associated default rules.

## Modular UI Architecture and State Management

### Separation of Concerns
The hardware operating state is strictly decoupled from its
presentation. The hardware (or digital twin) acts as the single source
of truth.

### Independent Widget Objects
UI widgets are independent PascalCase classes (`Preselector.lua`,
`AGC.lua`, etc.). The main UI instantiates these objects, which then
manage their own sub-widget hierarchies.

### Reactive State Graph
To keep the UI in sync with the hardware, the system relies on a
reactive property graph. Widgets use observers to watch the hardware
state and trigger re-draws only when necessary.

---

# Reactive Property System Design

A minimal reactive core for Lua to enable automatic dependency
tracking and change propagation for UI layout and configuration.

## Core Concepts

```
┌─────────────────────────────────────────────────────────────┐
│  Reactive Graph                                             │
│                                                             │
│  [Observable]──────►[Computed]──────►[Computed]             │
│       │                  │               │                  │
│       ▼                  ▼               ▼                  │
│  [Computed]         [Watcher]       [Watcher]               │
│       │              (UI update)    (DSP update)            │
│       ▼                                                     │
│  [Watcher]                                                  │
│                                                             │
│  On change: topological propagation, no glitches            │
└─────────────────────────────────────────────────────────────┘
```

## What This Gets You

| Feature | Included |
|---------|----------|
| Automatic dependency tracking | Yes |
| Lazy evaluation (compute on read) | Yes |
| Topological update order (no glitches) | Yes |
| Batched updates | Yes |
| Bidirectional computed | Yes |
| Watchers for side effects | Yes |
| Cleanup/disposal | Yes |
