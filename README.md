# One-Button Looper

One-Button Looper is a simple looper plugin with only one button. It mimics the working of the 
TC Electronic Ditto looper pedal.
Forked from: https://github.com/stevie67/loopor


Features:
* Stereo inputs and outputs
* Compiled in max number of overdubs (currently 128), a compiled max overall recording time (currently 6 minutes)
* Configurable input threshold; when starting the recording it can wait until a certain threshold is reached.
* Just one button to control all functions, saving switching real estate!
* No clicks even when sounds is still playing at loop end.
* Configurable amount of dry signal routed to the outputs.
* Optionally after first dub continue recording.


Usage:
* Adjust the "Threshold" to only start recording once playing has started. If set to the lowest value, recording will start immediately.
  Otherwise it will start recording when the first sound comes in. The threshold can be used to filter out noise. 
* Adjust the "Dry Amount" to reduce the volume of the input signal directly routed to the output. Setting it to 0 means you will only hear
  the looped sounds, no direct sound.
* Press the button to start recording the first dub. Press again to stop recording. The first dub's length will define the length
  of all loops. Recording of all but the first dubs will stop when the loop length is reached. However, this behaviour can be changed
  by setting the Continuous Dub parameter to ON. There is one exception: When the threshold is configured and no audio was recorded yet,
  then recording will not stop at the end of the loop.
* Press the button while playing to start overdubbing.
* While overdubbing, long press/hold the button to go back one dub. Undoing the first dub will also stop playing.
* While playing. long press/hold the button to redo a dub. Redoing is possible as many times as undo was used before. Redoing the first dub will start playing 
  again. Redoing is no longer possible when the next dub record is started! This will invalidate all undone dubs!
* Press the "Reset" button to stop recording if it is recording. Otherwise do the same as the "Undo" button.
* Double press the button at any time to reset the looper, which clears all loops.
* The parameter Continuous Dub controls if after the first dub recording continuous. The default is off, but with the on setting it behaves like
  most other loopers.
