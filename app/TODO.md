# NOTES to do

* Use SetBox animation/easing mechanism to implement all crossfades in
  audio and demodulator chain. This allows these to be controlled in a
  common way, lets SetBox control speed/type of easing, etc. We
  already have a mechanism to do this sort of thing, so adding another
  one for crossfading _ad hoc_ each time we need to do something like
  that is a waste and complicates the system.

* Find better ways of doing binding from C++ to Lua and back again to
  avoid messes like this:
  
	 } else if (name == "bandpassCenter" && std::is_same_v<T, double>) {
		 if constexpr (std::is_same_v<T, double>) rxConfig_->setBandpassCenter(val);
	 } else if (name == "bandpassWidth" && std::is_same_v<T, double>) {
		 if constexpr (std::is_same_v<T, double>) rxConfig_->setBandpassWidth(val);
	 } . . .

