# NexRx UI Architecture Specification: "The River"

**The River** is the primary spectral, temporal, and decoding
workspace of the NexRx High-Frequency Software-Defined Radio (SDR)
application. Rather than presenting radio spectrum as a traditional
downward-falling vertical waterfall paired with detached decoder
windows, The River rotates the coordinate system 90 degrees so that
**time flows horizontally from right to left** (`NOW` pinned on the
right, oldest history on the left) and **frequency maps vertically**
(lowest frequency at the bottom, highest at the top).

This removes the notion of the traditional "VFO" knob for tuning,
although incrementing tuning is still possible and a useful technique.
Instead, instant direct access to signals seen on the River and
digital readout of those signals' frequencies provides much of the
same capability without making the frequency band an inherently
sequentially accessed medium as is often the case with current
receivers.

Combined with a deep, wideband raw I/Q history buffer on the host PC
and an immediate-mode, 60+ FPS Lua/Raylib rendering engine, The River
transforms a 300 kHz ham band passband into an interactive,
text-searchable, time-shifting visual transcript.

---

## 1. Design Rationale: Why "The River" Exists

Traditional SDR interfaces inherit legacy analog metaphors—a
horizontal frequency dial at the top, a downward-scrolling waterfall
below it, and separate pop-up windows for CW, FT8, or RTTY decoding.
That layout introduces three fundamental operational bottlenecks that
The River resolves:

1. **Inline Left-to-Right Readability:** In a vertical waterfall,
   overlaying decoded text next to a vertical signal track requires
   either sideways text or detached floating boxes that obscure
   adjacent signals. By orienting time horizontally with **`NOW` at
   the right edge**, newly arriving RF energy appears on the right and
   scrolls leftward into history. As a station transmits `"CQ DE
   W1AW"`, the `'C'` arrives first and scrolls left as subsequent
   characters arrive to its right. Consequently, chronological signal
   arrival and Western left-to-right (LTR) reading order are 100%
   locked together, allowing decoded text to sit directly inline on
   top of the exact RF envelope that produced it. *(Note: Reversing
   the horizontal axis to put `NOW` on the left was evaluated and
   rejected, as pinning characters to timestamps scrolling rightward
   spells words backward from left to right).*


2. **Omniscient Wideband Capture vs. Narrowband Listening:** The NexRx
   STM32 hardware and C++ host ingest ~300 kHz of raw I/Q bandwidth
   (up to 384 kS/s)—enough to capture an entire amateur radio band
   (such as 20 meters) at once. Because a human operator can only
   listen to one or two narrow channels at a time, traditional radios
   discard everything outside the current tuning passband. The River
   records the full wideband I/Q stream into a host PC ring buffer (~2
   MB/s uncompressed, or ~200 MB for a 100-second rolling window) so
   operators can retroactively tune, filter, and play back signals
   they missed in real time.


3. **Decoupling Band Intelligence from Audio Focus:** In conventional
   radios, a signal is only decoded or identified if the VFO is tuned
   to it. In The River, background skimmers continuously decode and
   tag signals across the entire 300 kHz passband automatically;
   user-placed channel boxes (`SignalBox`) exist strictly for audio
   demodulation focus, passband filtering, and persistent manual
   bookmarking.

---

## 2. Spatial & Visual Anatomy of the Viewport

The River workspace is organized into coordinated zones that maximize
RF canvas space while keeping active manipulation controls and passive
status indicators strictly separated.

```text
                        [Top Time Ruler: -45s ... -35s]  |<-- 29s -->|   NOW
                      +---------------------------------+-------------+-------+--------------------+
[▲ Off-Screen Pile]   |                                 \             \       |  [S9+6] [][][][]   |
                      |                                 /             /       |  (Live S-Meter)    |
                      |                    (v)          \             \       |        |           |
[14245 kHz | NEW DX]  |====================#============/==CQ W1AW====/=======|>       }           |
                      |                    (|)          \             \       |>      }            |
                      |                    (^)          /             /       |        }  14240    |
                      |                     |           \             \       |       }   (USB)    |
                      |          [Amber Playback Line]  /             /       |        |           |
                      |                                 \             \       |       }   14120    |
[• 14120 Alert Tick]  | -- THERE IS A LOT OF QSB -------/-------------/-------|      }    (CW)     |
[▼ Off-Screen Pile]   +---------------------------------+-------------+-------+--------------------+
                      [     Scrolled History Region     ] [CAD Break]  [Pinned] [Live Trace &      ]
                                                                       [ Live ] [Shaded Freq Ruler ]

```

### The Main River Canvas & Optional Grid

* **Time-Frequency Matrix:** Each vertical pixel column represents a
  slice in time $t$ advancing leftward, and each horizontal row
  represents a frequency bin $f$. Signal strength is rendered via
  theme-able color/brightness mapping.


* **Zero-Cost Optional Grid Lines:** CAD-style time and frequency grid
  lines across the River are controlled via a theme color table. By
  default, grid lines can be hidden for an unencumbered canvas; if
  `alpha == 0`, the render loop skips the grid draw pass entirely for
  zero GPU/CPU cost.

### The Split-River & CAD Discontinuity Break Line

When `timeOffset == 0`, The River is a single continuous canvas flush
against the green `NOW` line on the right. When the operator scrolls
backward into history (`timeOffset > 0`), the layout dynamically
splits into three sibling regions so the operator never goes blind to
live band activity:

* **HistoryRegion (Left):** Displays the scrolled historical timeframe
  (e.g., `-45s` to `-31s`).


* **TimeBreakSeparator (Middle):** A subtle, muted CAD-style vertical
  zigzag line across the River indicating a temporal discontinuity.
  Above it in the top time ruler, standard **CAD dimension lines**
  (`|<--- 29s --->|`) with thin drop-extensions indicate the exact
  duration of the skipped time gap. When returning to live view, the
  zigzag vertices smoothly interpolate their amplitude down to `0.0`
  over ~250ms, pulling taut into a straight line and dissolving.


* **LivePinnedRegion (Right):** A 1-to-2-second sliver of real-time
  River pinned against the green vertical `NOW` boundary line (labeled
  with a tiny, vertically oriented `NOW` tag at the top). Clicking
  anywhere inside this pinned sliver immediately snaps the viewport
  and audio back to real time.



### Right-Hand Gutter: Live Trace, Dual-Decay S-Meter & Frequency Ruler

* **Tektronix Phosphor Live Trace:** Immediately to the right of the
  green `NOW` line, a real-time spectral amplitude curve protrudes
  horizontally (stronger signals extend further right). It renders
  using a two-pass additive OpenGL blend (wide low-alpha bloom +
  narrow white-hot core) with optional frame-to-frame persistence
  decay to emulate a classic Tektronix P31/P11 CRT oscilloscope.


* **Compact Horizontal Dual-Decay S-Meter:** Positioned cleanly above
  the live amplitude trace (replacing any oversized full-height
  vertical meter) is a single horizontal segmented bar with dual
  numeric readouts (`S9+6` slow-decay standard ham reading alongside a
  fast/instantaneous number). Individual colored segment boxes render
  **solidly filled with high saturation** up to the instantaneous
  signal level, while segments between the instantaneous level and the
  slow-decay peak render as **hollow box outlines in the same color**
  (slightly dimmed).
* **Subtly Shaded Frequency Ruler:** Along the far right edge, the
  vertical frequency scale (`freqMin` to `freqMax`) integrates
  band-plan boundaries (`CW`, `DATA`, `USB`, etc.) as subtle
  background shading or a thin muted tint directly behind the ruler
  ticks, eliminating the need for a dedicated label column.

### Left-Hand Gutter: Carrier Tabs, Alert Ticks & Off-Screen Piles

* **Carrier-Aligned Frequency Tabs:** Each active `SignalBox` displays
  a frequency readout tab along the left edge (`14245 kHz`) aligned
  strictly with the signal's **virtual carrier / zero-beat frequency**
  (bottom edge for USB, top edge for LSB, center for CW/AM) rather
  than the geometric center of the passband box.
* **Unboxed Ambient Alert Ticks:** Signals decoded in the background
  without a `SignalBox` can place compact colored ticks along the left
  edge when they trigger user-defined alert rules.
* **Top/Bottom Off-Screen Alert Piles:** When zoomed in vertically on
  frequency, high-priority alerts and unfocused `SignalBox` tabs
  located outside the visible `[freqMin, freqMax]` window clamp and
  stack neatly into priority-ordered piles at the top-left (`▲`) and
  bottom-left (`▼`) corners. Hovering previews the off-screen
  stations; clicking spring-pans the frequency axis directly to that
  signal.

---

## 3. Core Functional Capabilities & Operator Workflows

### Tier 1: Always-On Ambient Decoding & Bubble Zoom Loupe

Background skimmers in the C++ DSP engine continuously monitor the
entire 300 kHz passband for CW, FT8, RTTY, JS8, and voice peaks
without requiring a `SignalBox`:

* **Adaptive Level of Detail (LOD):** When zoomed in sufficiently,
  decoded text renders inline directly over the signal's horizontal
  track at a fixed, readable font size. When zoomed out in frequency
  or time such that inline text would collide vertically or crush
  horizontally, tracks automatically step down into compact,
  color-coded activity ribbons.


* **Cursor-Following Bubble Zoom (Decoder Loupe):** Hovering the mouse
  cursor over crowded or zoomed-out decoded tracks activates a local
  magnifying bubble that follows the cursor. Inside the bubble,
  anamorphic 2D scaling expands both vertical frequency spacing and
  horizontal time spacing so closely packed streams (such as the FT8
  sub-band or a dense CW contest pileup) separate into crisp, readable
  text lines. Mouse clicks pass straight through the magnified
  coordinate transform to drop a `SignalBox` or start playback at that
  exact `(timestamp, frequency)`.

### Tier 2: `SignalBox` Audio Focus, Vernier Handles & Time-Shifted Playback

A `SignalBox` is spawned or moved by clicking on the River (or
`Shift+Click` to add multiple boxes, cycling focus with `TAB` and
`Shift+TAB`):

* **Context-Aware Auto-Tuner & Manual Override:** When dropped onto a
  signal, the C++ DSP engine analyzes the local 2D time-frequency
  morphology to automatically select the demodulation mode (`CW`,
  `USB`, `LSB`, `RTTY`) and adjust passband skirts to slice out
  adjacent QRM (`autoTuned` tag).


* **Outward-Facing Bracket Handles & Vernier Dragging:** Only the
  currently focused `SignalBox` displays manipulation handles at the
  right edge. Rather than centered diamonds that overlap when a box is
  only 2–3 pixels tall at a 300 kHz span, the handles are
  **outward-facing brackets/chevrons** sitting in fixed screen-pixel
  hit zones immediately *above* the upper skirt and *below* the lower
  skirt (paired with cursor shape changes). Dragging a handle switches
  to a fine-vernier scaling mode (`manualOverride` tag) so operators
  can dial in 10–25 Hz filter adjustments even when zoomed all the way
  out.


* **Time-Machine Audio Playback & `travelingScoped` Cursor:** Clicking
  inside a `SignalBox` further to the left in history drops a vertical
  amber playback cursor and immediately routes historical demodulated
  audio from that exact moment to the headphones. Riding directly on
  the moving playback cursor—vertically scoped to the focused
  `SignalBox`—is a local Tektronix-style phosphor trace and a
  single-bar dual-decay S-meter showing the instantaneous historical
  signal strength at the playback head.


* **50% Spring-Slide Page Jumps & Time-Stretch Catch-Up:** When the
  rightward-walking playback cursor reaches the CAD break line on a
  scrolled River, the `HistoryRegion` executes a 150ms
  critically-damped spring slide forward by 50% of the visible history
  width (configurable via Lua) to keep the cursor in view. Optional
  pitch-locked time-stretch (`1.25x`–`1.5x`) and squelch-gap skipping
  allow the playback cursor to catch up to the green `NOW` line and
  seamlessly weld back into real-time monitoring.



### Semantic Callsign Naming, HamDB Enrichment & Shareable Lua Award Rules

* **Temporal QSO State Naming:** As decoders extract callsigns on a
  frequency, the `SignalBox` label dynamically reflects the state of
  the QSO *at that point in time*: displaying `CQ <callsign>` during a
  calling phase, and transitioning to show both participants
  (`<callsign1> / <callsign2>`) once a respondent turns the QSO over.
  Operators can also manually type a custom name to lock the label.
* **Asynchronous HamDB & Local Cache Enrichment:** Extracted callsigns
  trigger a non-blocking lookup against a persistent local cache on
  the host PC, falling back to stateless HTTP JSON queries to
  **HamDB.org** (plus over-the-air grid extraction and offline
  DXCC/prefix tables). Hovering over a `CQ <callsign>` box displays a
  tooltip card with operator details, grid square, great-circle
  distance (`km`/`mi`), and beam heading; hovering over a full two-way
  QSO displays a dual-station card with details for both parties.
* **Shareable Lua Contact & Contest Flagging:** Enriched callsign and
  grid metadata is passed through user-customizable Lua matcher
  scripts that compare the station against the operator's log. Scripts
  emit semantic camelCase tags (`newToMe`, `heardPreviously`,
  `workedPreviously`, `neededDX`, `neededWAS`, `neededWAC`) that
  color-code the left-edge carrier frequency tab, badge the inline
  callsign, or dim duplicate stations. Hams can freely trade and share
  these rule scripts without altering core application code or
  personal visual themes.



### Long-Term Binary Band Activity Log & Inspector

* **Compact Binary Storage:** While raw wideband I/Q is bounded to a
  rolling RAM/disk ring buffer, all decoded signal metadata (UTC
  timestamp, frequency, mode, SNR, callsigns, grids, and matched tags)
  is appended to a compact, fixed-stride binary log file on the host
  PC. Memory-mapped (`mmap`) binary access keeps months of band
  history searchable in sub-millisecond time at minimal disk cost.
* **UI Browser & CSV Export:** A built-in Band Activity Inspector lets
  operators scrub, search, and filter historical band openings by
  callsign, grid, mode, or award tag, click recent entries to rewind
  the River I/Q buffer, or export filtered views to `.csv` text files
  on demand.

---

## 4. Integration into the NexRx Application Architecture

The River is designed around a strict separation of responsibilities
across three architectural tiers, adhering to a **camelCase /
PascalCase** symbol convention (with `UPPERCASE` reserved for acronyms
like `DSP_PAYLOAD`):

| Architectural Tier | Primary Responsibilities in "The River" |
| --- | --- |
| **1. NexRx STM32 Firmware** | Real-time RF digitization and high-speed streaming of raw dual-OSD I/Q samples (`DSP_PAYLOAD` at up to 384 kS/s) to the host PC. Completely unburdened by UI state or historical buffering.

 |
| **2. C++ Host Engine (DSP, Storage & Raylib Bridge)** | • Maintains the user-configurable raw I/Q RAM/disk ring buffer (~2 MB/s).

<br>

<br>• Runs wideband FFTs, multi-mode background skimmers
(CW/FT8/RTTY/Voice), and context-aware `SignalBox` signal
classification.

<br>

<br>• Manages asynchronous HamDB REST workers, local callbook caching,
and `mmap` binary metadata logging.<br>

<br>• Exposes immediate-mode OpenGL/Raylib drawing primitives
(`RenderBridge`) and SDL2 multi-touch/mouse events to Lua.

 |
| **3. Declarative Lua UI Engine** | • Defines decoupled container layouts (`HistoryRegion`, `TimeBreakSeparator`, `LivePinnedRegion`) and independent 2D zoom/pan state (`timeSpan`, `timeOffset`, `freqMin`, `freqMax`).

<br>

<br>• Resolves visual styling via the hierarchical tag tree (`Local ->
Container -> Window -> Global`).

<br>

<br>• Executes user-customizable **Behavior Strategy Tables** (such as
`RiverBehaviors.travelingScoped` and `BubbleBehaviors.adaptiveLoupe`)
and shareable award/contest tagging scripts at 60+ FPS.

 |

### Why Tag-Parametric Strategy Tables Matter

Rather than hardcoding UI behaviors behind rigid C++ enums or mode
strings, The River encodes interactive behaviors—such as how the
historical playback trace renders (`renderPlaybackTrace`), how far the
viewport jump-scrolls when hitting the CAD break line
(`calculateJumpTarget`), how the bubble zoom loupe activates
(`shouldActivateBubble`), and how callsigns are flagged for awards—as
**Lua tables containing first-class functions**. Users and plugin
authors can inherit from default behavior tables via Lua metatables
(`__index`) and hot-swap individual functions or visual theme
properties at runtime, achieving deep customization while keeping the
underlying C++ DSP and rendering loop lean and deterministic.
