# Changelog

All notable changes to Curvable are listed here. Versions follow [Semantic Versioning](https://semver.org).

## [0.2.0]

### One editing mode

- The Draw, Curve and Slice tools are gone. Every action is available at any time: hovered and selected notes show their bend points and handles, double-click a note to add a point, and right-click a note for its menu.
- Drag a note's left end to change where it starts, Shift + drag to duplicate, right-drag to delete, and hold S to slice.
- Vibrato is edited on the note: pull up the amber line above a note to add it, add points to the line to shape the depth over time, and drag the blue handle to change the speed. Depth can go down to zero. The shaped line replaces the fade-in bar.
- The bottom bar explains whatever is under the mouse, and a card shows what is new the first time a version is opened.

### Volume and pan

- Every note can have its own volume curve and pan curve, drawn in the VOL and PAN lanes below the notes. Curvable applies them to the output of each synth copy, so they work with any synth.
- Ready-made fade in, fade out and swell. Shapes are copied to all selected notes, stretched to each note's length.
- Slicing, trimming and gliding keep the curves in place. They have no effect in MPE mode and are not included in MIDI export.

### Playback

- Curvable can play its notes while the host is stopped: press Space or the play button. Click the ruler to set the start, drag in it to set a loop, and turn on follow to scroll with the playhead. The host takes over when it starts playing.
- Dragging a bend point plays the note at that pitch.
- A setting lets Space control FL Studio instead.

### Other

- Curvable checks GitHub for new releases at most once a day and offers the installer when one is out. It can be turned off in the settings menu.
- The logo opens the project's page on GitHub.

## [0.1.0]

First public release.

### Piano roll and curves

- Piano roll with Draw, Curve and Slice tools, snapping, velocity lane, undo and redo, copy and paste, and keyboard shortcuts.
- A pitch curve for every note, with adjustable curve shape between points.
- Vibrato and trill on the whole note or on selected segments, each segment with its own settings, edited with on-note handles for depth, speed and fade-in. Speed in Hz or locked to the tempo.
- Ready-made bends: scoop, overshoot, bend and release, fall and doit.
- Curve presets shared across projects, stretched to each note's length.
- Glide: merge a line of notes into one legato note, or glide between chords voice by voice.
- Slice tool that keeps curves unchanged.

### Hosting

- Hosts a VST3 instrument (and AU on macOS) and embeds its interface.
- Runs copies of the synth so up to 16 notes can bend independently. Copies are created and removed automatically, pause when idle, and stay in sync with the main synth.
- MPE mode for synths with MPE support.
- 16 automation slots that link host automation to the synth's knobs.
- Finds instruments already scanned by FL Studio, and loads plugins dropped from FL Studio's browser.

### Playback and exchange

- AUTO, TRIGGER and OFF playback modes.
- MIDI import with pitch bends converted to curves.
- MIDI export by dragging or saving, as one channel per voice or as MPE.

### Interface

- English, Chinese (Simplified), Japanese, Korean and Spanish.
- Light and dark themes; resizable window.
