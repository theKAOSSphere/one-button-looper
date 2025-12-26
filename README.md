# One-Button Looper

Forked from: https://github.com/stevie67/loopor

One-button looper plugin for LV2, specifically for the MOD devices software ecosystem.

NOTE: No warranty for anything is given! Use at your own risk (see the license).

## Features:
* Stereo inputs and outputs
* Compiled in max number of overdubs (currently 128), a compiled max overall recording time (currently 6 minutes)
* Configurable input threshold; when starting the recording it can wait until a certain threshold is reached.
* Single button with Ditto-style functionality (record, overdub, stop, undo/redo, clear)
* No clicks even when sounds is still playing at loop end
* Configurable amount of dry signal routed to the outputs

## Usage:
* Adjust the "Threshold" to only start recording once playing has started. If set to the lowest value, recording will start immediately.
  Otherwise it will start recording when the first sound comes in. The threshold can be used to filter out noise.
* Adjust the "Dry Amount" to reduce the volume of the input signal directly routed to the output. Setting it to 0 means you will only hear
  any looped sounds, no direct sound.
* Press Once: Starts recording the loop.
* Press Again: Ends the recording and starts playback immediately.
* Press Once (during playback): Starts overdubbing. This allows you to layer additional parts over the original loop.
* Press Twice (quickly): Stops playback or recording.
* Press and Hold: Clears the loop when playback is stopped.
* Undo/Redo: While overdubbing, pressing and holding the footswitch undoes the last overdub. Pressing and holding again restores the overdub (redo).