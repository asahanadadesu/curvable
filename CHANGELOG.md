# Changelog

All notable changes to Curvable are listed here. Versions follow [Semantic Versioning](https://semver.org).

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
