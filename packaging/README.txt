H7600 Control
=============

Remote control for the Eventide H7600 over MIDI SysEx: preset browser,
live parameters and an LCD mirror. Run time only: nothing is ever saved
on the unit. Source, documentation and unit setup:
https://github.com/jdesfossez/h7600-control

Installing
----------
macOS    Standalone/H7600 Control.app  -> /Applications
         AU/H7600 Control.component    -> ~/Library/Audio/Plug-Ins/Components
         VST3/H7600 Control.vst3       -> ~/Library/Audio/Plug-Ins/VST3
         The builds are not notarized: the first time, right-click the app and
         choose Open, or run: xattr -dr com.apple.quarantine "<file>"
Windows  Standalone/H7600 Control.exe runs as is
         VST3/H7600 Control.vst3       -> C:\Program Files\Common Files\VST3
Linux    Standalone/H7600 Control runs as is
         VST3/H7600 Control.vst3       -> ~/.vst3

On the H7600, SETUP > MIDI: MIDI enabled, SYSTEM EXCLUSIVE on, MIDI map = none,
SEQUENCE OUT = new (optional), and enter the unit's DEVICE ID in the app.

Licences
--------
This project's code: MIT (LICENSE.txt). These binaries include the JUCE
framework under the AGPLv3, so the binaries as a whole are AGPLv3; the source
is at the address above. The VT323 font is under the SIL Open Font License
1.1 (VT323-OFL.txt). Eventide and H7600 are trademarks of Eventide Inc.; this
project is not affiliated with Eventide.
