# Rules to Follow

Don't write code that does conditional tests in the main loop for,
e.g., "useHwSpectrum" and related logic or "waterfall.isInitialized()"
and instead use for each case a variable whose value is a function,
initialized to the fallback or no-op case at start up and then
modified by changing of conditions that affect these behaviors. In
general, stop using "if" where a function pointer change tracking the
conditions can be used instead - especially if the "if" condition or
the conditionalized code within the if/else relate to something we
will use setbox rules to control.
