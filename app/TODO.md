# NOTES to do

* Use SetBox animation/easing mechanism to implement all crossfades in
  audio and demodulator chain. This allows these to be controlled in a
  common way, lets SetBox control speed/type of easing, etc. We
  already have a mechanism to do this sort of thing, so adding another
  one for crossfading _ad hoc_ each time we need to do something like
  that is a waste and complicates the system.

* add the missing ham bands.

* Find better ways of doing binding from C++ to Lua and back again to
  avoid messes like this:
  
	 } else if (name == "bandpassCenter" && std::is_same_v<T, double>) {
		 if constexpr (std::is_same_v<T, double>) rxConfig_->setBandpassCenter(val);
	 } else if (name == "bandpassWidth" && std::is_same_v<T, double>) {
		 if constexpr (std::is_same_v<T, double>) rxConfig_->setBandpassWidth(val);
	 } . . .

* TODO: do I need bidirectional bindings like QML?


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
