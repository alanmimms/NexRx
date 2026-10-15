NexRx UI Architecture Guide

## Overview

NexRx is a high-frequency (HF) amateur radio receiver built on a novel
hardware design. It streams dual OSD I/Q samples (up to 384 kS/s)
via a 480Mb/s USB connection to a host PC.

The software architecture divides responsibilities strictly to
maintain high performance while offering maximum flexibility. The host
application is built in C++ to handle the last stages of digital
signal processing (DSP) and real-time data constraints. The user
interface, however, is driven entirely by Lua 5.4, utilizing Raylib
and SDL2 to render an immediate-mode, OpenGL-accelerated GUI that
operates similarly to a high-framerate video game UI.

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

### Theming and State (Tags)

State is managed, by default, through a hierarchical tag system.
However any state value can be set to a Lua function, so this can be
overridden. By default the state values are set to the Lua function
that implements this hierarchical tag system. In the tag system,
themes are defined globally using normalized RGBA color values (0.0 to
1.0). When a widget renders, it resolves its style by traversing its
tag hierarchy (Local -> Container -> Window -> Global).

This allows a global state change (e.g., adding a `"transmitting"`
tag) to instantly alter the appearance or behavior of deeply nested
leaf widgets, such as checkboxes or sliders, without requiring tight
coupling.

### Layout Strategies

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
adhesion to container boundaries via the `props.stick` table. This
allows `stick.left` and/or `stick.right` and equivalent for top and
bottom to be used to keep a given widget attached to its adjacent
neighbor.


#### Proportional Mapping (Data-Driven Placement)

For SDR-specific widgets, like a `SignalBox` representing a
demodulation target on the spectrum, positioning is mapped directly
from the domain data (frequency) to screen space, bypassing
traditional flex or grid rules.


### The C++ Bridge

The C++ host provides primitive rendering functions and handles
high-throughput data. To maintain performance and follow project
conventions, the API utilizes standard C strings (`const char*`)
rather than standard library string objects, avoiding unnecessary
allocations during the render loop.

Functions are generalized, and templates are reserved strictly for
handling varying DSP data types.

# UI Notes

## Terminology

* A signal box is a unique bandwidth, frequency, and demodulation mode
  to be applied to a signal on the waterfall.
  * There may be any (reasonable) number of signal boxes, but only one
    can be the focused at once.
  * TAB key switches among the signal boxes, and shift-TAB switches
    among them in the reverse order.
  * Demodulation focus (or simply focus) is current signal box the
    receiver's audio and S meter are focused on.

* The Waterfall is also called the River, which flows from the right
  toward the left.

## Layout

* Make S-meter be vertical along the right or left edge of the live
  waveform upper part of the waterfall with decaying peak and text
  below showing peak S*s*+*d*dB reading. Text display dB value isn't
  limited by size of graphical area.


## Mouse Signal Manipulation

* Drag bars appear when cursor hovers over focus box to:
  * Drag bandwidth left or right edge.
  * Drag top to change waterfall vertical scale.

* Drag bars appear when cursor hovers over edges of waterfall.
  * Left and right edges allow scaling up and down (zoom) of
    waterfall's view of passband.
  * At seam between live waterfall signal wave and waterfall below,
    allows adjusting height of split of display.
	

## Waterfall Legend

* Legend box shows waterfall grid units:
  * Vertical scale (dB, 3dB, etc.) in live upper portion.
  * Vertical scale (10ms, 100ms, 1s, 10s) in history lower portion.
  * Horizontal scale (1, 10, 100, 1k, 10k, 100k, 1M, 10M).
	
## Cursor Shape

* When cursor is over waterfall history, cursor changes shape to
  indicate it's possible to play back history buffered signal.

* When cursor is over signals box not focused, cursor changes shape to
  a target to show click would focus at that point's frequency.
  * When cursor is over area not currently in a signal box, cursor
    changes shape to show a signal box can be created there.
  * If shift is held down, a new signal box is created by clicking.
  * If shift is not held down, current signal box is abandoned and new
    frequency becomes the new focused signal box.
  * Some keystroke (control-Z?) undoes this effect to allow a mistaken
    click to be undone.

## Undo

* Control-Z undoes:
  * Signal box change or creation.
  * Change to bandwidth or height of focus.
  * Change to waterfall split or zoom.
* Control-Y redoes a mistaken undo operation.

## Auto Tune Mode

* "Auto-tune mode", which is probably enabled or disabled or applied
  when a modifier key is held down when creating a new signal box,
  tunes the frequency precisely to match the signal under the click.
  The signal's modulation mode is determined automatically.

  * CW sets the frequency so the signal's tone is the user's preferred
    sidetone frequency.

  * Single sideband automatically chooses the right sideband, sets the
    frequency to the precise carrier frequency of the signal.
	* Should this update bandwidth to match signal?

  * AM and FM modes tune to center the signal's carrier and modulation
    in the signalbox passband.

## Scrolling and Zooming

* Scrolling slides current signal box up and down in frequency.
* Zooming changes bandwidth displayed, resizing frequency ruler.
* Zooming with shift key changes time displayed, resizing time grid.

## Context Menu

* Right click on frequency ruler gets you popup to change frequency
  manually and to change band.
  
## Keyboard Shortcuts

* TAB moves between signal boxes, and shift-TAB moves in the opposite order.

* ENTER brings up the ruler context menu dialog to change frequency
  manually and/or select a different band.

* Control-Z undo.
* Control-Y redo.

* M is audio MUTE toggle.

* Arrow up and down change volume level.

* Arrow left and right allow history audio playback forward from a
  given point moved by the arrow direction in steps of 25% of a time
  grid square.

## Possibilities

* Consider flipping time to horizontal.
