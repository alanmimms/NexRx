# Unified Tag & SetBox Architecture

## 1. Executive Summary

NexRx utilizes a **Digital-First** architecture where application state, UI layout, and hardware control are unified into a single reactive system. There is no hardcoded layout engine; instead, **SetBoxes** contain **Rules** that match **Tags** to derive **Properties**. Drawing occurs from first principles every frame, ensuring the UI is always bit-identical to the underlying configuration.

## 2. Core Principles

* **Everything is a Widget**: Every visible element (text, waterfalls, buttons) is a widget responding to the same tag-based lifecycle.
* **Behavior via Rules**: Styling and event handling are declared in SetBoxes, not hardcoded in Lua.
* **Reactive Synchronicity**: UI properties are **Reactive Observables**. A change in a SetBox property triggers an immediate topological reflow of the UI.
* **Live-Round-Trip**: The UI can be manipulated graphically (Design Mode) to update SetBox properties, which are then serialized back to Lua files.

## 3. The Tag System

Tags are namespaced strings representing the current environment. The **Active Tag Set** is recomputed every frame from five sources:

| Namespace | Source | Examples |
| --- | --- | --- |
| `widget.` | Widget Identity | `widget.VfoA`, `widget.Waterfall` |
| `state.` | App/Hardware State | `state.Mode-USB`, `state.Hardware-Twin` |
| `input.` | Held Inputs | `input.SHIFT`, `input.MouseLEFT` |
| `event.` | Transient Events | `event.MouseDown-LEFT`, `event.KeyDown-H` |
| `layout.` | Structural Forces | `layout.Anchor-Left`, `layout.Spring-H` |

### Comparison Syntax

Rules can match against widget properties using comparison operators:

* `tag.prop==value` (Equals)
* `tag.prop>value` (Greater than)
* `tag.prop~A,B,C` (Value in set)

## 4. SetBox Rules & Scoring

A **SetBox** is a hierarchical configuration container. It contains rules that apply properties when a specific combination of tags is active.

### Priority and Overrides

Each tag in a rule can have a priority suffix `@N` (default `@1`).

```lua
-- Rule 1: Global Default
rule { tags = {"widget.Button"}, apply = { bg = "#374151" } } -- Score = 1

-- Rule 2: Hover State (Higher priority)
rule { tags = {"widget.Button@10", "state.Hovered@5"}, apply = { bg = "#3b82f6" } } -- Score = 15

```

The rule with the highest total score (sum of tag priorities) determines the property value. This allows child SetBoxes to override parent behavior by providing more specific or higher-priority rules.

## 5. Reactive Property Integration

Properties are not static values; they are **Computeds** in the Reactive Property System.

* **Lazy Evaluation**: Properties are only re-calculated when a dependency (like a changed SetBox value) is updated.
* **Topological Updates**: If the `layout.SidebarWidth` changes, all dependent widgets (VFOs, Waterfalls) reflow in the correct order to prevent visual "glitches".

## 6. Layout via Attraction Forces

Instead of rigid Docks or Splits, NexRx uses an **Attraction Force Model**. Layout is determined by reactive "springs" and "anchors" defined in rules:

* **Anchors**: Fixed relationships to container edges (e.g., `anchorLeft = 0`).
* **Springs**: Flexible distribution of space (e.g., `springHorizontal = 1.0`).
* **Reserving Space**: Widgets use the `layout.reserveSpace(w, h)` property to influence the cursor of their parent container.

## 7. The Live-Round-Trip (Design Mode)

The "Live-Round-Trip" allows the GUI and Lua code to be manipulated interchangeably.

1. **Enter Design Mode**: Activating `state.DesignMode` triggers rules that add visual handles (borders, resize grips) to all widgets.
2. **Manipulation**: Moving a widget via `event.MouseMove` updates the corresponding **Observable** in the active SetBox.
3. **Reflow**: The reactive core detects the change, updates the layout properties, and the UI draws the new position in the next frame.
4. **Serialization**: A reactive **Watcher** monitors the SetBox and writes the updated Lua rule definitions back to the project repository.

## 8. Event Bubbling & Handlers

Events are transient tags. When an event occurs:

1. **Tag Injection**: Transient tags like `event.MouseDown-LEFT` are added to the Active Set.
2. **Bubbling**: The event includes tags from the target widget and all parent containers.
3. **Handler Execution**: The highest-scoring rule with a `handler` property executes its Lua function.
4. **Completion**: If the handler returns `true`, the event is consumed; otherwise, it continues to bubble.

## 9. Implementation Standards

* **Naming**: CamelCase for methods/variables, SNAKE_CASE for constants.
* **Performance**: Targeting >= 60fps on host hardware for smooth manipulation.
* **Asynchronicity**: UI rendering is asynchronous to DSP; structural changes do not interrupt the I/Q stream.

---
