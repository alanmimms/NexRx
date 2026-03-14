# NexRx: User Experience & Features
## Interface Design, Workflows, and Operating Features

### The Browser-Native Interface

Opening NexRx feels nothing like traditional radio software. There's no installation process, no driver hunting, no compatibility concerns across operating systems. Plug in the USB cable, open Chrome or Safari, navigate to the captive portal, and you're immediately presented with a modern, responsive interface that feels more like a professional audio application than a ham radio.

The interface adapts to your screen size and usage patterns. On a large monitor, you might have multiple waterfall displays showing different zoom levels, constellation plots for digital modes, and audio analysis tools. On a tablet, the interface reorganizes to prioritize the most commonly used controls while keeping advanced features accessible through intuitive gestures.

### Waterfall-Centric Operation

The waterfall display serves as far more than just a spectrum visualization - it becomes a primary control surface. Mouse wheel scrolling tunes frequency with pixel-level precision, while modifier keys enable different functions directly on the graphical display.

**Zooming**: The spectrum and waterfall can be zoomed horizontally to examine signals in high detail or to see the entire 96 kHz bandwidth at once. Zooming is controlled by the `+` and `-` keys (defined via SetBox rules). The system maintains a fractional zoom level, ensuring that zooming out never exceeds the point where the spectrum fills the available display width. Zooming specifically affects the frequency (horizontal) axis and automatically updates the graticule legend and all active SignalBoxes.

**SignalBoxes**: NexRx introduces the concept of **SignalBoxes**—visual representations of active tuned signals. Unlike a traditional radio with a single VFO, NexRx allows you to create multiple SignalBoxes on the spectrum (using the `/` key).
- Each SignalBox tracks its own frequency, mode, and bandwidth.
- Clicking a SignalBox selects it, making it the "active" receiver controlled by the VFO and mode buttons.
- SignalBoxes can be dragged across the spectrum to retune.
- Ghost SignalBoxes (temporary previews) allow you to see where a new box will be placed before committing.
- When zooming, SignalBoxes automatically recalculate their visual size and position to remain locked to their tuned frequencies.

### The SetBox Generalization Paradigm

NexRx is powered by **SetBox**, a rule-based configuration and styling system that treats every aspect of the interface—from the color of a button to the logic of a frequency readout—as a resolvable property. The power of this mechanism comes from its **consistent applicability** and **strict rule-driven nature**. 

#### Pure Spring-Constraint Layout Engine



NexRx completely eschews traditional fixed-coordinate positioning and legacy "anchoring" mechanisms in favor of a unified **Pure Spring-Constraint Layout**. The UI is treated as a transient physical system in equilibrium, calculated dynamically every frame in two distinct passes.

**Pass 1: Incompressible Minimums (Bottom-Up)**
Every UI element (labels, checkboxes, groups) calculates its own "floor" size based on its content, where leaf widgets have a fixed size based on their implementation and current rule resolution. The layout engine is prohibited from shrinking any element below this limit. A parent compound widget calculates its minimum size by accumulating the minimums of its children. This guarantees that even on small screens, the UI remains legible and interactive.

**Pass 2: Spring Force Distribution (Top-Down)**
Instead of binary "stickiness," the layout is governed entirely by **Spring Stiffness** values (`springTop`, `springBottom`, `springLeft`, `springRight`) assigned to every widget by SetBox rules. The actual "gap" spring between two sibling widgets is the average of their opposing spring strengths. 

Once the parent's total available space is determined, the "Surplus" (or Deficit) padding is distributed to the gaps and widgets based on these physical properties:
* **Infinite Stiffness (`math.huge`)**: Acts as a rigid, unyielding link. A widget with `math.huge` spring to its left edge is mathematically "stuck" to it. The gap takes 0% of the surplus space.
* **Positive Stiffness ($S > 0$)**: An elastic spring. The layout engine distributes available surplus proportional to the *reciprocal* of the stiffness ($1/S$). Softer springs (lower values) expand more eagerly to fill space.
* **Neutral Stiffness ($S = 0$)**: Passive geometry. The element only moves if pushed or pulled by surrounding forces.
* **Negative Stiffness ($S < 0$)**: A repulsive force ("The Pusher"). It actively attempts to expand its gap, pushing adjacent widgets away.

**The Pivot Rule (Overflow Handling)**: If the window is too small, the "Surplus" becomes negative. To prevent unsolvable layouts or squished text, widgets remain at their incompressible minimums. The engine anchors the layout at the Top/Left edges (0,0) and negative springs/deficit space force the trailing widgets to extend cleanly off-screen, maintaining usability for primary controls.

#### 1. No Hardcoded Defaults
In NexRx, there are no "default values" buried in the source code. If a widget needs to know its border width, its background color, or even the text it should display, it must ask SetBox. If no rule matches the current context, the system explicitly fails. This enforces a discipline where every behavior and visual element is declared in configuration files, making the entire application a "blank canvas" for the user.

#### 2. Local Widget Context (LWC)
Every UI element is a standalone object (e.g., `Button.lua`, `Slider.lua`) that operates within a **Local Widget Context**. When a widget is drawn, it creates an LWC that combines:
* **Specific Identity**: A unique ID tag (e.g., `id.vfo-slider`).
* **Generic Type**: A widget type tag (e.g., `widget.Slider`).
* **Hierarchical Parentage**: Inherited tags from its parent container (e.g., `widget.Sidebar`).
* **Global State**: Global tags like the current band (`tag.20m`) or theme (`theme.Dark`).

#### 3. Rule Resolution: Specificity Over Priority
SetBox uses a sophisticated resolution engine where **specificity** (the number of matching tags) takes precedence. There are no arbitrary integer priorities. This allows for surgical overrides.

* A rule matching `{"widget.Button"}` (specificity 1) sets the global button style.
* A rule matching `{"widget.Sidebar", "widget.Button"}` (specificity 2) overrides that style only for buttons inside sidebars.
* A rule matching `{"id.rx-toggle", "state.Active"}` (specificity 2) can change the text and color of one specific button only when it is engaged.

#### 4. Practical Layout Examples

By combining SetBox Specificity with Spring Physics, complex responsive UI behaviors emerge naturally without procedural code.

**Example A: The Centered VFO Display**
We want the main frequency display to remain dead-center in the top bar, regardless of window size.
* We add a spacer widget on the left and right of the VFO.
* SetBox rule `{"widget.TopBar", "widget.Spacer"}` assigns a soft elastic spring (`springX = 1.0`).
* SetBox rule `{"id.MainVFO"}` assigns infinite internal stiffness (`springX = math.huge`) so it doesn't stretch, and calculates its incompressible minimum based on the font size.
* *Result:* The VFO stays at its minimum size, and the two soft spacers equally absorb 100% of the window's surplus width, perfectly centering the VFO.

**Example B: The Responsive Sidebar**
We want a sidebar that stays rigidly attached to the right edge on large monitors, but gracefully pushes off-screen when the window is too narrow (e.g., `< 800px`).
* Global state emits a `tag.NarrowScreen` tag when the window shrinks.
* Base rule `{"widget.Sidebar"}` sets `springRight = math.huge` (anchoring it to the right edge) and `springLeft = 10.0` (resisting expansion).
* Override rule `{"widget.Sidebar", "tag.NarrowScreen"}` sets `springRight = -math.huge` (explosive repulsion).
* *Result:* Because specificity 2 beats specificity 1, the moment the screen narrows, the sidebar's right spring becomes repulsive. It actively pushes itself rightward, seamlessly sliding off-screen while the rest of the UI (which uses top-left anchoring via the Pivot Rule) remains perfectly intact.

#### 5. Generalized Content
Even UI strings are generalized. A `Label` widget can resolve its `text` property from a rule, or use a dynamic callback for real-time data like frequency. For instance, the "ACTIVE TAGS" header is not a hardcoded string; it is a property resolved by a `Label` widget matching the `activeTags.title` tag. This makes the UI entirely localizable and customizable without touching a single line of procedural code.

#### 6. Tag-Driven Interaction & Focus
NexRx eschews traditional discrete "focus" state variables. Instead, it employs a **dynamic focus-by-hover** model driven by the same SetBox tag resolution used for styling.

* **Dynamic Context Propagation**: As the mouse moves across the interface, the system identifies the widget under the cursor and propagates its entire set of tags (type, ID, and groups) to the global active tag list.
* **Implicit Focus Rules**: Interactive behaviors are defined by rules that intersect an input event with these propagated tags. For example, a rule matching `{"event.KeyDown-Digit", "widget.VFOControl"}` handles frequency entry. Because the Spectrum and Waterfall widgets also carry the `widget.VFOControl` tag, they implicitly "focus" the VFO when hovered without requiring any procedural state management.
* **Reactive Highlighting**: Visual feedback follows the same logic. A frequency display highlights itself only when it detects that its group or identity tags are currently global (meaning its control area is hovered) or when specific state tags like `state.VFOEditing` are active.
* **Event Gobbling by Priority**: The event dispatch system bubbles through the hierarchy, allowing specific rules to "gobble" events. This allows complex modes—like the frequency entry mode—to hijack keyboard input by providing high-priority handlers that only trigger when the correct environmental tags are present.

### Setbox Workflow in Practice

Imagine you're preparing for the ARRL DX Contest and want to set up configurations for different bands and modes.

**Setting Up Base Configurations**: Start by creating a "Contest-Base" setbox rule that defines common contest settings—reduced font sizes for a compact UI, specific antenna selections, and your contest callsign.

**Band-Specific Adjustments**: Create rules for `Contest-20m` and `Contest-40m` that inherit from the base but add band-specific AGC settings optimized for weak signal work versus pile-up busting.

**Live Operation**: Switching bands or modes instantly re-evaluates the SetBox context. The UI instantly reflects new colors, labels, and even layout adjustments because every widget's `:draw()` method re-queries its LWC for its current properties.

### Advanced Visualization and Analysis

Because significant DSP processing happens in the browser, NexRx can provide visualization and analysis tools that would be impossible in traditional hardware radios.

**Constellation Diagrams**: For digital modes like PSK31 or FT8, real-time constellation plots provide immediate feedback on signal quality and decoding performance.

**Audio Analysis Tools**: Switchable oscilloscope and spectrum analyzer views of demodulated audio help with precise adjustment of receive and transmit audio equalization.

### Memory and Logging Systems

Traditional memory channels become much more powerful in the setbox paradigm. Instead of just storing frequency and mode, a NexRx memory can capture the complete operating state - antenna selections, DSP settings, power levels, even waterfall color schemes.

**Tagged Memory System**: Memories include arbitrary tags, colors, creation timestamps, and usage tracking. You might tag memories with "DX", "Contest", "Ragchew", or "EmComm" and then filter displays to show only relevant memories for your current activity.

**Complete State Recording**: The system maintains a chronological log of all state changes with replay capability. This feature proves invaluable for debugging equipment problems or analyzing propagation changes.

### Development and Customization

The SetBox architecture makes customization and enhancement much more accessible than traditional radio firmware modification.

The open-source nature means the entire ham community can contribute improvements, from bug fixes and feature additions to completely new interface paradigms that leverage the flexible setbox foundation. Because widgets are standalone objects that rely strictly on rules, adding a new UI component is as simple as defining a new class and its associated default rules.

## Modular UI Architecture and State Management

### Separation of Concerns
The hardware operating state is strictly decoupled from its presentation. The hardware (or digital twin) acts as the single source of truth.

### Independent Widget Objects
UI widgets are independent PascalCase classes (`Preselector.lua`, `AGC.lua`, etc.). The main UI instantiates these objects, which then manage their own sub-widget hierarchies.

### Reactive State Graph
To keep the UI in sync with the hardware, the system relies on a reactive property graph. Widgets use observers to watch the hardware state and trigger re-draws only when necessary.

---

# Reactive Property System Design

A minimal reactive core for Lua to enable automatic dependency tracking and change propagation for UI layout and configuration.

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

The reactive graph effortlessly drives the Spring Layout engine. The SetBox LWC acts as the `[Observable]`. When tags change (e.g., resizing the window or swapping SetBoxes), the `[Computed]` spring values update automatically based on specificity rules. This topological propagation signals the `[Watcher]` (the Layout Engine) to cleanly execute the two-pass layout algorithm without glitches.

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
