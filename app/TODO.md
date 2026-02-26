# NOTES to do

* Lose `local bandFreqs` in `main.lua` and use the globally defined
  full band table.

* Use SetBox animation/easing mechanism to implement all crossfades in
  audio and demodulator chain. This allows these to be controlled in a
  common way, lets SetBox control speed/type of easing, etc. We
  already have a mechanism to do this sort of thing, so adding another
  one for crossfading _ad hoc_ each time we need to do something like
  that is a waste and complicates the system.

* Reimplement `if freqEntryMode then` code in `update()` in `main.lua`
  to use setbox tags for frequency entry mode and steering keystrokes
  to the freq entry processing code.
  * Define many/most SDL scan codes for key strokes in a separate lua
    file.
* Change mouse wheel tuning actions to be setbox based dispatched with
  modifiers.
* Remove ESC handling and switch to control-Q.
  * Allow this anywhere.

* Distribute `fonts/DejaVuSans.ttf` if possible according to license.
  * Fix fonts so they don't create a mess of complaints at start.

* Add tag for platform the app is running on so we can conditionalize
  things like font path using proper rules.

# UI
* Make mode reflected in selector for mode and remove it from status bar.
* Make band reflected in selector for band and remove it from status bar.
* Eliminate status bar and/or move some status to title bar.

* Create named widgets for:
  * Current VFO freq, including draggable to change it with delta buttons
	* +100,+1k,+10k,+100k,+1M and negate by using shift key or control key.
	* Need obvious way to type in a frequency to tune to.
  * VFO selector
    * Use drag/drop to drag freq to a "VFO" storage cell and/or create a new one.
  * S meter
	* Needs to support vertical and horizontal orientation.
	* Could be docked onto side(s) of waterfall.
  * Waterfall
  * Current spectrum
  * Band
  * Filters
  * AGC
  * Noise reduction controls
  * Volume
    * Includes MUTE somehow.
  * Squelch
  * RF gain/attenuation
  * Color scheme editor
    * Generic, not just for waterfall, but with SetBox applicability tags
  * FPS meter
  * Any text
    * Needs "NexRx" as title
  * Recording controls
  * Logging controls
  * App needs a real icon

## How to do each use scenario
* Show and change bandpass and notch filters on spectrum/waterfall.
* Zoom in and out for waterfall/spectrum width.
* Show sideband rejection.
* Create and change color schemes.
* Move widgets around and set up layout rules.
* Set up SetBox rules and tags and try them out.
* Tie various controls to keystrokes and/or mouse clicks and wheel with modifiers.
  * E.g., volume up and down are up/down arrows and freq is left/right arrows.
  * SetBox for each event type with modifiers as part of name.
    * SetBox sets whatever effect the event is tied to.
	* Can be specific to mouse position or not via event delivery search path.
* Right click or similar "context menu".
  * For volume control -> MUTE.
  * S meter decay time and peaking behavior.
  * Add/change/delete SetBox associated with a UI item.
* Zoom out to show more than +/-50kHz waterfall/spectrum.
  * Can timeshare or quick-shift tuning for QSDs somehow?
* Can waterfall look much more like spectrum rotated out from 2d on screen into 3d?



# Document

The design philosophy is:

  1. All UI is presentation only.
     * We handle events based on tags defined for UI regions, for clicks, keys, modifiers, etc.
	 * The UI widget code creates a presentation.
	 * The code that creates a UI widget by calling that code also
       creates the setbox rules to handle events as needed for that
       UI.

  1. All events dispatched uniformly - clicks, releases, motion, wheel, keys
  2. Tags describe the event context - mouse button type, held mouse buttons, modifiers, widget, mode
  3. SetBox rules determine handlers - no hard-coded mouse button-specific behavior
  4. Motion events are first-class - with held mouse buttons acting as modifier

* Need capability to support future extension of set of UI devices.
  * Can add USB based keypads, touchpads, knobs, etc. for example like the ones used for CAD.

# Hardware Calibration & Verification (BIST)
* Implement FPGA BIST Stimulus:
  * Add command to FPGA to generate a square wave on IOB_22a at a requested frequency.
  * Filtered through 130pf + 100k resistor, fed to rx preselector in 200 ohm domain.
  * Consider if FPGA can generate a better signal than square wave (e.g., simple DDS or PWM filtering).
  * Add commands to Twin/HW to tune and toggle this BIST signal.
* Preselector Calibration Sweep:
  * App uses FPGA BIST to inject signal, QSD to measure amplitude.
  * App sweeps all 11 capacitors (C0..C10) and Inductor L1 across frequencies.
  * App computes the optimal L and C bit pattern for every passband center frequency to ensure monotonic, overlapping coverage.
  * App sends this a priori calibration table to Twin/HW for storage.
  * Replace real-time solver with a priori calculation using this calibration data.
* Attenuator Calibration:
  * App uses FPGA BIST to inject steady signal.
  * App switches each attenuator stage (3, 6, 12, 24dB) in and out, measuring actual amplitude drop.
  * App computes true attenuation for each combination.
  * App generates calibration data mapping requested dB to the most accurate bit setting.
* File Server Mechanism:
  * Implement file transfer protocol over TCP link between App and HW/Twin.
  * Support updating: FPGA image, NexBus STM32C011 firmware (up to 16 KBytes).
  * Execute STM32H753 firmware in place (no tiny bootloader). Recovery via standard USB firmware update mechanism.
  * Implement maintenance ops: READ, WRITE, SHA256, DELETE, LIST.
  * Store files in a highly efficient Flash file system optimized for small data files (like calibration data) on STM32H753 flash (and local filesystem for Twin).
