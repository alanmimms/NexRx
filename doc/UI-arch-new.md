# NexRx PC Host UI — Unified Architecture & Source Reference

The NexRx PC Host UI is a reactive, rule-driven software-defined radio
(SDR) interface built in Lua on top of a C++ host providing rendering
(`System`), DSP (`rx`, `audio`, `waterfall`), and hardware (`hw`)
bindings.

---

## 1. Core Architectural Discipline

The UI architecture is governed by four interconnected layers that
separate **data state**, **contextual rule resolution**, **widget
identity**, and **retained-mode layout**:

```
[ Config Rules & Tags ] ──► [ SetBox & Hierarchical LWC ]
                                   │               │
                                   ▼               ▼
[ C++ / Hardware HAL ] ◄──► [ Reactive Model ]   [ Widget.props (Reactive Proxy) ]
                                   │               │
                                   ▼               ▼
                            [ Retained Widget Tree (Layout & Draw) ]

```

### 1.1 Reactive Primitives (`Reactive.lua`)

All dynamic state flows through a lazy dependency graph built on three
primitives:

* **`R.observable(initialValue)`**: Stores mutable state and notifies
  dependents when updated. Calling `:get()` registers a dependency
  with the currently evaluating computed property, while `:peek()`
  reads the value without tracking.


* **`R.computed(fn)`**: Derives values lazily from observables or
  other computeds, recomputing on read only when an upstream
  dependency is marked dirty.


* **`R.watch(fn)` & `R.batch(fn)**`: `R.watch` executes side effects
  immediately when tracked dependencies change, while `R.batch` groups
  multiple state mutations into a single notification cycle.



### 1.2 Canonical Widget Identity & Automatic Tagging

Every widget in the retained UI tree has a single canonical identity
and an automatically managed set of `SetBox` tags:

* **Unified `id` and `name**`: In `Widget:init(def)`, a widget
  resolves its identity via `self.id = def.id or def.name or
  (def.props and def.props.id) or Widget.newID()`, with `self.name`
  aliasing `self.id`.


* **Automatic Hierarchy Tagging**: When a widget initializes its Local
  Widget Context (`self.lwc`), it inherits from `parent.lwc` and
  automatically registers:


* **Identity tag**: `"id." .. self.id`.


* **Class hierarchy tags**: `"widget." .. typeName` for the widget's
  concrete type and each `super` class in its `Widget.mkType`
  inheritance chain (e.g., `widget.Column` and `widget.Container`).


* **Explicit tags**: Any custom domain tags passed in `def.tags` (such
  as `"widget.VFOControl"` or `"widget.Sidebar"`).




* **Reactive Interaction State Tags**: Focus, hover, and
  active/pressed states managed by `Widget.updateGlobalMouse`,
  `Widget:setFocus`, and `ui.State` directly toggle `"state.Hovered"`,
  `"state.Focused"`, `"state.Pressed"`, and `"state.Active"` on
  `self.lwc` rather than assembling temporary tag arrays during
  `drawSelf()`.



### 1.3 Reactive `SetBox` & Local Widget Context (`LWC`)

`SetBox` evaluates property rules based on specificity (number of
matching tags), priority score, and declaration order:

* **Observable Tags**: Global tags (`activeTags`) and local context
  tags (`Context.localTags`) are stored as dictionary maps of `tagName
  -> R.observable(boolean)`. Checking `Context:hasTag(tag)` reads
  `:get()` on the tag's observable (walking up `self.parent` to the
  global `SetBox` state), automatically subscribing the caller to that
  tag.


* **Reactive Rule Versioning**:
  `Context:getMatchingRules(propertyName)` reads
  `getPropertyVersion(propertyName):get()` (or
  `globalRulesVersion:get()` when resolving all properties). Whenever
  a rule for `propertyName` is added or replaced, only computations
  depending on `propertyName` are invalidated.



### 1.4 Unified Widget Properties (`self.props`)

The standalone `WidgetProps` concept and `ui.Widget`'s `self.props`
and `self.metrics` tables are unified into a single reactive property
proxy on every `Widget` instance:

1. **Local Computed Override**: If a widget defines a computed
   property via `self.props:computed(key, fn)` (including cross-widget
   layout formulas), reading `self.props[key]` returns
   `computed:get()`.


2. **Local Observable Override**: If a property is explicitly set on
   the widget (via `def` initialization or `self.props[key] = value`,
   such as `x`, `y`, `w`, `h`, or `text`), reading `self.props[key]`
   returns `observable:get()`. Setting `self.props[key] = nil` clears
   the local override.


3. **Cached `SetBox` Rule Fallback**: When no local computed or
   observable override exists for `key`, `self.props[key]` lazily
   creates and evaluates a per-widget `R.computed(function() return
   self.lwc:get(key) end)`. Styling properties (`background`,
   `border`, `foreground`, `opacity`) and layout metrics (`flexW`,
   `flexH`, `prefW`, `prefH`, `minW`, `maxW`, `margin`, `stick`) are
   all accessed through `self.props`, updating automatically when tags
   or rules change without per-frame uncached rule lookups.

---

## 2. Source File Catalog & Responsibilities

### 2.1 Core State, Rules & Event Infrastructure

* **`Reactive.lua`**: Implements `observable`, `computed`,
  `observableObject`, `watch`, `batch`, and `untrack` for lazy
  reactive dependency tracking across the model and UI.

* **`SetBox.lua`**: Rule-based configuration and styling engine. Loads
  rule scripts (`SetBox.loadFile`), tracks global and hierarchical
  `Context` (`LWC`) tags reactively, and resolves properties by
  specificity, priority, and declaration order.

* **`Model.lua`**: Structured reactive domain model acting as a live
  projection of `SetBox` rules. Exposes receiver state (`Model.rx`),
  multi-slice `signalBoxes`, `spectrumCenterFreq`, `filters`, `ISG`,
  and `waterfall` settings. Calling `Model.set(name, value)` creates
  or updates a priority-1000 override rule in `SetBox` and increments
  the property's reactive version.

* **`Events.lua`**: Central event dispatcher and handler registry.
  Maps raw input events into transient `event.*` tags (e.g.,
  `event.MouseWheel`, `event.KeyDown-*`), queries the target widget's
  `lwc` for a matching `handler` property from `SetBox` rules, and
  invokes the registered handler function.

* **`Keycodes.lua`**: Maps numeric Raylib key codes and modifiers
  (340–348) to human-readable key names (`"SPACE"`, `"ENTER"`, `"F1"`,
  etc.) for event routing.

* **`Animate.lua`**: Keyframe and `A -> B` property tweening engine
  with easing functions (`linear`, `easeIn`, `easeOut`, `cubicInOut`,
  `elasticOut`, `backOut`), ticked each frame by `update(dt)`.

### 2.2 Radio Domain & Hardware Control Plane

* **`main.lua`**: Application entry point. Loads `config/*.lua` rule
  files into `SetBox`, initializes `AppController`, `Calibration`,
  `Bands`, `Events`, and hardware connections, constructs the root
  `rxTree` widget hierarchy, and exports the global `UI` callback
  table (`render`, `onResize`, `onMouseMove`, `onMouseEvent`,
  `onKeyEvent`, `onTextInput`) plus `update(dt)` for the C++ host.

* **`AppController.lua`**: Business logic and synchronization
  coordinator. Registers `Events` handlers (`vfo_control`,
  `freq_entry_*`, `sb_select`, `sb_drag_*`, `sb_next`, `sb_name_*`),
  watches `Model` observables via `R.watch` to mark subsystems dirty,
  polls hardware state (`pollState`), and flushes batched changes to
  `Hardware.sync()` once per frame.

* **`Hardware.lua`**: Unified Hardware Abstraction Layer (HAL)
  communicating with the C++ `hw`, `rx`, `audio`, and `waterfall`
  bindings. Deduplicates outgoing state commands via `lastSent` in
  `Hardware.sync(commands)` and provides spectrum/waterfall rendering
  wrappers.

* **`Bands.lua`**: Amateur band management module. Loads band
  boundaries from `SetBox` rules tagged with `"BandDef"` (or falls
  back to 160m–10m defaults) and automatically swaps the active band
  tag in `SetBox` whenever `Bands.setCurrent(freqHz)` detects a band
  transition.

* **`Modes.lua`**: Single source of truth for demodulator mode names
  (`"USB"`, `"LSB"`, `"AM"`, `"CW"`, `"BYPASS"`) and 0-based integer
  ID mappings for `rx.setModeId` / `rx.getModeId`.

* **`Calibration.lua`**: Loads and persists OSD0/OSD1 gain, phase, and
  IQ alignment offsets in `config/calibration.lua`, applying them to
  hardware via `hw.setCalibration`.

* **`PropertyHandlers.lua`**: Table-driven property change dispatcher
  (`_G.onPropertyChange`) that maps configuration keys (`volume`,
  `muted`, `mode`, `colormap`, `bandpass*`, `notch*`, `osdOffsetK`,
  `rfAttenuation`) to C++ `audio`, `rx`, `waterfall`, and `hw` calls.


### 2.3 Retained-Mode UI Framework (`ui/*`)

* **`ui/Widget.lua`**: Base class and object factory (`Widget.mkType`)
  for all UI elements. Manages parent-child trees (`kids`), localized
  coordinate rendering (`SystemWrapper` and `WaterfallWrapper`),
  hit-testing (`hitTest`), focus/hover tracking, `SetBox` context
  inheritance (`self.lwc`), and unified reactive properties
  (`self.props`). Also defines core primitives (`Container`, `Column`,
  `Row`, `Window`).

* **`ui/Layout.lua`**: Retained-mode container layout solvers
  (`Layout.hFlow`, `Layout.vFlow`, `Layout.zStack`, `Layout.wrapFlow`,
  `Layout.grid`). Performs two-pass measurement and positioning using
  child metric properties (`prefW`, `prefH`, `flexW`, `flexH`, `minW`,
  `maxW`, `margin`, and `stick`).

* **`ui/Stick.lua`**: Bitmask constants (`L=1`, `R=2`, `T=4`, `B=8`,
  `TLBR=15`, etc.) and string parser (`Stick.mk("TLR")`) controlling
  widget anchoring and cross-axis stretching inside layout containers.

* **`ui/State.lua`**: Tracks global interaction state (`hot`,
  `active`, `focus`), mouse coordinates, and the coordinate
  translation offset stack (`getOffset`) used during recursive widget
  tree rendering.

* **`ui/Color.lua`**: Parses hex color strings (`#RGB`, `#RGBA`,
  `#RRGGBB`) and normalized RGB tables into `Color` objects with
  manipulation helpers (`darken`, `toTable`).

* **`ui/TextMixin.lua`**: Shared text measurement helper
  (`TextMixin.measure`, `TextMixin.getLineHeight`) wrapping
  `System.measureText`.


### 2.4 Modular UI Widgets

All widgets inherit from `ui.Widget` via `Widget.mkType`, register
default `SetBox` styling rules at priority `-1000` under their
`"widget.<Type>"` tag, and participate in retained-mode container
layouts:

* **`ui/Panel.lua`**: Styled container background and border surface
  driven by `widget.Panel` rules.


* **`ui/Label.lua`**: Text readout widget supporting static
  `self.props.text`, dynamic `getText` closures, and `Stick`-based
  alignment.


* **`ui/Button.lua`**: Clickable action control with `widget.Button`
  default styling and reactive hover/active state tags.


* **`ui/Checkbox.lua`**: Boolean toggle control styled via
  `widget.Checkbox` rules.


* **`ui/Slider.lua`**: Defines continuous `Slider` (bound to a numeric
  `valueObs` or `propertyName` over `[minVal, maxVal]`) and
  `Slider.DiscreteSlider` for categorical stops such as `Modes` and
  `Bands`.


* **`ui/FrequencyDisplay.lua`**: Interactive VFO readout
  (`widget.FrequencyDisplay`) supporting mouse-wheel stepping,
  arrow-key tuning, and direct keyboard frequency entry
  (`state.FreqEntryMode` with `M`/`K` unit suffixes).


* **`ui/Spectrum.lua` & `ui/SignalBox.lua**`: `Spectrum` renders
  real-time FFT data via `Hardware.renderSpectrum`, overlays a
  `GraticuleLegend`, converts between pixel coordinates and Hz
  (`getFreqAtPx`, `getPxAtFreq`), and manages child `SignalBox`
  widgets representing draggable passband slices from
  `Model.signalBoxes`.


* **`ui/Waterfall.lua`**: Renders the scrolling spectrogram history
  via `Hardware.renderWaterfall` synchronized with `Model.waterfall`
  zoom and dB range settings.


* **`ui/SMeter.lua`**: Calculates RMS-to-dBm and S-unit readings
  (`S1`–`S9+dB`) from `rx.getSignalRms()` and renders a segmented
  multi-color LED bar graph (`widget.SMeter`).


* **`ui/ActiveTags.lua`**: Diagnostic viewer
  (`widget.ActiveTagsViewer`) displaying currently active `SetBox`
  tags.


* **Composite Control Modules (`AGC.lua`, `AudioUtils.lua`,
  `CWUtils.lua`, `ISG.lua`, `ReceiverControl.lua`)**: Modular control
  groups for AGC mode selection, audio mute/test-tone toggles, CW
  pitch configuration, Internal Signal Generator controls, and
  receiver controls. Under the unified discipline, these modules
  compose child widgets (`Label`, `Button`, `Checkbox`, `Slider`,
  `FrequencyDisplay`) as retained `Container` subtrees
  (`Widget.Column` / `Widget.Row`) rather than positioning children
  via immediate-mode cursor calls.

---

## 3. Consolidated & Retired Legacy Modules

To keep the codebase free of competing patterns, the following legacy
modules and patterns are folded into the core system or retired:

* **`widget_props.lua` (`WidgetProps`)**: Folded directly into
  `ui/Widget.lua` as the per-instance `self.props` proxy backed by
  `self.lwc`. Its unit test suite (`WidgetPropsTest.lua`) is updated
  to test `Widget.props` directly alongside `ReactiveTest.lua`.


* **Immediate-Mode Region Layout (`ui/Layout.lua` cursor/region
  stack)**: Replaced entirely by the retained-mode container layout
  solvers (`Layout.hFlow`, `Layout.vFlow`, `Layout.zStack`,
  `Layout.wrapFlow`, `Layout.grid`).


* **Duplicate Property Version Tables**: `Model.lua` previously
  maintained its own private `propertyVersions` table separate from
  `SetBox.lua`. `Model.lua` now delegates directly to `SetBox`'s
  reactive property version tracking so rule mutations and `Model.set`
  overrides share a single notification path.


* **Disabled Layout Editing Stubs (`edit.lua` & `constraints`)**:
  Removed in favor of reactive `SetBox` metric rules and
  `ui/Layout.lua` flex/stick solvers.
