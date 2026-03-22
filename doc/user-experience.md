# NexRx: User Experience & Features
## Interface Design, Workflows, and Operating Features

### The Native Interface

Opening NexRx feels like starting a professional audio workstation. It
is a high-performance native application built in **Lua 5.4** and
**Raylib**, providing a hardware-accelerated, OpenGL-rendered
interface that responds with the fluid precision of a modern game
engine.

The interface is fully responsive, adapting seamlessly to large
high-resolution monitors or compact tablet screens. It prioritizes
high-framerate visualizations of the RF spectrum and waterfall,
ensuring that signal discovery and monitoring are as smooth as
possible.

### Waterfall-Centric Operation

The waterfall display serves as far more than just a spectrum
visualization—it is the primary control surface for the radio.

**Zooming**: The spectrum and waterfall can be zoomed horizontally to
examine signals in high detail or to see the entire bandwidth at once.
Zooming is controlled by the `+` and `-` keys. The system maintains a
fractional zoom level, ensuring that zooming out never exceeds the
point where the spectrum fills the available display width. Zooming
specifically affects the frequency (horizontal) axis and automatically
updates the graticule legend and all active SignalBoxes.

**SignalBoxes**: NexRx uses **SignalBoxes**—rectangular windows on the
spectrum that represent active tuned signals.
- A SignalBox is highlighted when selected and dim when inactive.
- The selected SignalBox is the source of demodulated audio; frequency
  and mode changes apply to it.
- **Dragging**: SignalBoxes can be dragged across the spectrum to
  retune the demodulator. Dragging to the edges of the display scrolls
  the spectrum center frequency and warps the mouse to keep the signal
  centered.
- **Naming**: Pressing the double-quote (`"`) key starts naming the
  selected SignalBox, with full support for arrow keys, backspace, and
  cursor navigation.
- **Navigation**: The `Tab` key cycles through onscreen SignalBoxes,
  while `Shift-Tab` includes offscreen boxes in the rotation.

### The Unified Widget & Layout System

NexRx uses a structured, object-oriented UI system where every element
is an independent **Widget**. This system manages its own hierarchy,
focus, and interaction logic, providing a more robust and predictable
experience than traditional immediate-mode or tag-driven GUIs.

#### Flexible Layout Strategies

Instead of hardcoded coordinates, the UI employs a **Unified Layout
Engine** that processes widget hierarchies based on swappable
strategies:

*   **Stick-based Alignment**: Widgets use "stick bits" (Top, Left,
    Bottom, Right) to anchor themselves to container edges or
    siblings. This allows for classic stretching and pinning
    behaviors.
*   **Flex Distribution**: Containers can distribute surplus space
    among children using proportional flex values (`flexW`, `flexH`),
    allowing sidebars and control panels to resize intelligently.
*   **Domain-Driven Mapping**: Specialized widgets like the `Spectrum`
    use direct frequency-to-pixel mapping for SignalBoxes, ensuring
    they remain perfectly locked to their tuned RF frequencies
    regardless of layout changes.

#### State & Focus Management

Input handling and focus are managed directly by the Widget hierarchy:
*   **Hit Testing**: The system performs recursive hit testing to
    identify the deepest widget under the mouse for motion and click
    events.
*   **Explicit Focus**: Keyboard focus is tracked explicitly, allowing
    widgets like frequency displays or naming fields to "capture"
    input only when active.
*   **Event Bubbling**: Events bubble from the leaf widgets up through
    their parents until handled, allowing for sophisticated
    interaction patterns with minimal boilerplate.

### SetBox: Configuration & Styling

While the core UI logic is managed by the Widget system, the
**SetBox** paradigm remains the backbone of NexRx's configuration and
styling. SetBox acts as a hierarchical, rule-based property provider
that decouples *values* from *implementation*.

#### 1. Decoupled Styling
If a widget needs to know its border width, background color, or font
size, it queries its **Local Widget Context (LWC)**. SetBox resolves
these queries by matching the widget's ID, type, and state tags
against global configuration rules. This allows for complete UI
skinning and theming without touching procedural code.

#### 2. Hierarchical Configuration Inheritance
Traditional radios force operators to adjust many independent
parameters when changing bands or modes. In NexRx, a **SetBox** is a
named configuration profile (e.g., "Contest-20m-CW"). These profiles
form inheritance hierarchies:

*   **Global-Defaults** defines baseline AGC and audio settings.
*   **Contest-Base** inherits from defaults and adds a contest
callsign.
*   **Contest-20m-CW** inherits from its parent and overrides only the
frequency, filter width, and mode.

Switching profiles instantly re-evaluates the entire system
state—antenna selections, DSP parameters, volume levels, and even UI
colors—ensuring the radio is perfectly tuned for the current task in a
single click.

#### 3. Reactive Property Graph
The hardware state is synchronized with the UI through a **Reactive
Property Graph**. When a hardware parameter changes (like an AGC level
or S-meter reading), the change propagates through the graph,
triggering only the necessary UI updates. This ensures the interface
is always a live, accurate projection of the hardware state.


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

The reactive graph effortlessly drives the Spring Layout engine. The
SetBox LWC acts as the `[Observable]`. When tags change (e.g.,
resizing the window or swapping SetBoxes), the `[Computed]` spring
values update automatically based on specificity rules. This
topological propagation signals the `[Watcher]` (the Layout Engine) to
cleanly execute the two-pass layout algorithm without glitches.

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
