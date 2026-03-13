# List of ideas to work in someday or soon

* Add "auto" tuning to click on a signal in spectrum, analyze what
  that signal is with DSP, etc., and tune to hear it properly, setting
  demod mode, bandwidth, precise frequency to tune it, etc.

  * Add a "auto clean" to narrow bandpass filters, add notch(es) to
    eliminate interference, maybe find best noise blanking technology
    to use for the current signalbox.

* Build minimize solution for parts of the UI.

* Build detent-set based sliders for setting eg AGC mode instead of
  the four buttons. Should this be used for volume? RF gain?
  
* RF gain is currently a small integer in control commands. Why isn't
  this a 0..1 float? Attenuator blocks aren't the only thing
  controlled by it. PGA and audio codec permit fine grained gain.

* Add mechansism to tune bandpass width.

* Implement AGC

* Implement zoom for spectrum/waterfall

* Consider greater sample rates

* Change CALIBRATE to "I/Q balance cal" in utilities subUI

* Build notch filter solution - especially for CW over top of SSB and vice versa.

* Build decoder plugin solution.

* Build CW decoder.

* Build RTTY decoder.

* Build SSTV decoder.

* Build FT8 decoder.

* Use bands.lua data to create bands list for buttons.

* Use a new modes.lua to create modes list for modes.
