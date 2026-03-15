# UI Requirements
* Easy config of hierarchy of widgets.
* Flexible 2d layout based on various strategies depending on context.
* Preserve ability to do drag-n-drop UI editing.
* Everything in close context and not spread across separate
  conceptual frameworks.
* Stay in Lua if at all possible.

## SignalBoxes
* Are signalboxes (demodulator states really) a set of dynamically
  allocated properties of Spectrum or separate entities entirely?

## DND Editing
* Have to use constraint properties and not just functions that do
  layout.

* Need to be able to predict a "ghost" for where dnd would drop.

## Layout
* Is there advantage to reusing CSS concepts or is this a legacy best
  left behind?

## Rules and Properties
* Can metatables be used for powerful inheritance and conditional
  model instead of tags?
  * Evidently yes.

## Lua
* Don't forget about

	local <const> foo = 1234
	local <close> toBeClosed = factoryNew("xyzzy")
