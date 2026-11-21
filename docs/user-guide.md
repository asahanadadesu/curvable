# Curvable user guide

**English** | [简体中文](user-guide.zh-CN.md)

This guide describes Curvable 0.2. For installation, see the [README](../README.md#installation).

- [How Curvable works](#how-curvable-works)
- [Loading a synth](#loading-a-synth)
- [The window](#the-window)
- [Playback modes](#playback-modes)
- [Playing inside Curvable](#playing-inside-curvable)
- [Writing notes](#writing-notes)
- [Pitch curves](#pitch-curves)
- [Vibrato and trill](#vibrato-and-trill)
- [Ready-made bends and presets](#ready-made-bends-and-presets)
- [Glide](#glide)
- [Slicing notes](#slicing-notes)
- [Volume and pan](#volume-and-pan)
- [Pitch bend range](#pitch-bend-range)
- [Synth copies](#synth-copies)
- [MPE mode](#mpe-mode)
- [MIDI import and export](#midi-import-and-export)
- [Automation slots](#automation-slots)
- [Settings and updates](#settings-and-updates)
- [Mouse and keyboard reference](#mouse-and-keyboard-reference)
- [Troubleshooting](#troubleshooting)

## How Curvable works

A synth receives pitch bend per MIDI channel, so every note on that channel bends together. FL Studio sends all notes of an instrument on one channel. Curvable works around this in two ways:

- **Synth copies (default).** Curvable hosts your synth and creates more copies of it when needed. Notes that bend differently at the same time are played on different copies, so each copy's pitch bend serves one note. Notes that don't bend, or that bend identically, share a copy.
- **MPE mode.** If the synth supports MPE, Curvable can use a single copy and send each bending note on its own MIDI channel.

You write the notes and curves in Curvable's own piano roll. Curvable plays them in time with the host, or on its own, and the synth produces the sound.

## Loading a synth

Click **Load synth** at the top right. The synth list shows:

- synths you have loaded in Curvable before, most recent first;
- instruments that FL Studio has already scanned (read from FL Studio's plugin database, without loading them).

Type to search, then click a synth or press Enter. You can also:

- drag a `.vst3` file, or an instrument from FL Studio's browser, onto Curvable;
- click **Browse for file...** to pick a `.vst3` file;
- open the **Plugin manager...** to scan folders for plugins.

Once loaded, the button shows the synth's name. Click it again to switch to another synth or to unload it. The synth and its settings are saved with your project.

If a project refers to a synth that can't be found on this computer, the button shows **Missing: <name>**. Curvable keeps the synth's saved settings, so nothing is lost when you open the project on a computer that has it.

## The window

The top bar contains:

- The Curvable logo, which opens the project's page on GitHub.
- **Piano roll / Synth** tabs. The Synth tab shows the hosted synth's own interface. Edit sounds there as usual; Curvable passes your changes to every copy.
- **AUTO / TRIGGER / OFF**, the [playback mode](#playback-modes).
- **MPE**, the [MPE mode](#mpe-mode) switch, shown when a synth is loaded.
- The synth button, which also shows how many copies are running (see [Synth copies](#synth-copies)).
- The settings button (gear).

The piano roll has a keyboard on the left, a ruler at the top, the note area, and a lane at the bottom that shows velocity (**VEL**), volume (**VOL**) or pan (**PAN**). The bottom bar holds the play, loop and follow buttons, **Snap**, **Bend**, the **MIDI** button and **?** (all mouse and keyboard actions).

There are no tools to switch between: every action is available at any time. The text at the right of the bottom bar explains whatever is under the mouse, for example what dragging a handle does, and shows warnings.

The window can be resized by dragging its lower-right corner.

## Playback modes

- **AUTO** plays Curvable's notes in time with the host's timeline. In FL Studio, the Curvable channel plays when its pattern or the song plays. Notes from FL Studio's own piano roll for this channel are also passed to the synth, without bends.
- **TRIGGER** starts playback when the channel receives a note, for example from a note in FL Studio's piano roll or from a MIDI keyboard. Playback loops for as long as the note is held. The triggering notes themselves make no sound. Use this to place a Curvable phrase in a pattern as a single note.
- **OFF** does not play Curvable's notes. Notes from the host still reach the synth.

## Playing inside Curvable

Curvable can play its notes while the host is stopped, so you can listen to a curve as you edit it.

- Press **Space**, or click the play button at the left of the bottom bar, to start and stop.
- **Click the ruler** to set where playback starts. The start is marked in the ruler, and the playhead returns there when playback stops.
- **Drag in the ruler** to set a loop. Drag the ends of the loop to change it, and **double-click** the ruler to remove it. The loop button turns looping on and off. Right-click the ruler to loop the selected notes.
- The follow button scrolls the view along with the playhead.
- While you drag a bend point, Curvable plays the note at the point's pitch.

When the host starts playing (in AUTO mode), or a trigger note arrives (in TRIGGER mode), the host takes over. The start, the loop and whether it is on are saved with the project.

If you would rather have Space start FL Studio while Curvable's window has focus, turn on **Space plays FL Studio, not Curvable** in the settings menu.

## Writing notes

- Click an empty spot to add a note; keep dragging to set its length. New notes use the length of the note you last drew or resized.
- Drag a note to move it in time and pitch. Drag its right end to change its length, or its left end to change where it starts. The note's curves stay where they were in time.
- **Shift + drag** a note to duplicate it.
- Right-drag across notes to delete them. (Right-click opens the note's menu.)
- Hold **Alt** while dragging to move or resize without snapping.
- Click a key on the keyboard to hear the pitch.

Selecting notes:

- **Ctrl + drag** draws a selection box. **Shift + click** adds a note to the selection or removes it.
- **Ctrl + A** selects all notes; **Esc** clears the selection.
- **Delete** deletes the selection. **Ctrl + C / X / V** copy, cut and paste (pasting at the mouse position). **Ctrl + B** or **Ctrl + D** duplicates the selection right after itself.
- **Up / Down** transposes by a semitone (**Shift**: an octave). **Left / Right** moves by the snap step (**Shift**: a bar).
- **Ctrl + Z / Ctrl + Y** undo and redo.

Velocity: choose **VEL** at the left of the bottom lane and drag a bar, or hold **Alt** and turn the mouse wheel over a note. When the note is selected, the whole selection changes.

**Snap** sets the grid for notes and curve points: bar, 1/4 to 1/32, triplets, or none.

Navigation: the mouse wheel scrolls up and down, **Shift + wheel** scrolls left and right, **Ctrl + wheel** zooms horizontally and **Ctrl + Shift + wheel** zooms vertically. The scroll bars work too.

## Pitch curves

Each note has a pitch curve that describes how far it bends from its own pitch over time. A note without a curve plays at its pitch.

Move the mouse over a note, or select it, to see its points and handles.

- **Double-click** a note to add a point, and keep dragging to place it. Points snap to semitones and to the grid; hold **Alt** for free placement.
- Drag a point to move it. While dragging, its offset is shown in semitones and you hear its pitch.
- Between two points there is a small circle. Drag it up or down to bend that segment from a straight line into a curve that starts slowly or quickly.
- Right-click a point to delete it. **Clear curve** in a note's right-click menu removes the whole curve.
- Before the first point, the note stays at the first point's pitch. After the last point, it stays at the last point's pitch.

A thin dashed line marks each bent note's original pitch. Points outside the pitch bend range are drawn in red; see [Pitch bend range](#pitch-bend-range).

## Vibrato and trill

When you move the mouse to just above a note, an amber line appears over it (over the part between the two points around the mouse, if the note has points). **Drag the line up** to add vibrato there; the further you drag, the deeper it gets.

Once a segment has vibrato, its amber line stays on screen:

- **Drag the amber line** up or down to change the depth. For a trill, it sets the neighbouring pitch.
- **Double-click the amber line** to add a point to it, then drag the point. The shape of the line is how the depth changes over time, so you can make the vibrato swell, fade out or come and go. Right-click a point to delete it. The depth can go down to zero.
- **Drag the blue handle** above the line left or right to change the speed: left is slower, right is faster.

Right-click a note for more:

- **Vibrato on this segment** and **Vibrato on whole note**.
- **Trill on this segment** and **Trill on whole note** alternate rapidly between the note and a neighbouring pitch.
- **Lock speed to tempo** switches the speed between Hz and note values. Right-click inside a vibrato or trill to change only that one.
- **Remove vibrato** (or **Remove trill**) removes all vibrato and trills from the note.

Every segment has its own settings, so one note can have, for example, a slow vibrato on one segment and a trill on the next. When you add vibrato or a trill to a segment next to one of the same kind, it takes over that segment's settings and the two play as one continuous vibrato. Segments with different settings start and fade out separately. Choosing the same menu item again removes it from the segment.

Defaults: vibrato runs at 5.5 Hz and fades in over half a beat. A trill alternates with the whole step above, once every 1/32 note, locked to the tempo. When the speed is locked to the tempo, it snaps to 1/4, 1/8, 1/8T, 1/16, 1/16T or 1/32. For a trill, this is the length of each alternation.

When you zoom in far enough, the note itself is drawn with its vibrato; otherwise the vibrato is drawn as a wave over the note.

## Ready-made bends and presets

A note's right-click menu also offers common bends:

- **Scoop**: slide in from below
- **Overshoot**: go past the pitch and settle back
- **Bend and release**: bend up and come back
- **Fall**: drop at the end
- **Doit**: flick up at the end

A bend at the start and one at the end can be combined. Applying a bend to a selected note applies it to the whole selection.

**Presets** stores your own curves:

- **Save curve as preset...** saves the right-clicked note's curve, including vibrato or trill settings, under a name. Saving under an existing name replaces that preset.
- Choose a preset from the menu to apply it to the clicked note, or to every selected note. Point positions stretch to each note's length; vibrato speed does not.
- **Delete preset** removes one.

Presets are shared by all projects and all instances of Curvable. They are stored in `CurvePresets.xml` in Curvable's settings folder (Windows: `%APPDATA%\asahanada\Curvable`; macOS: `~/Library/Application Support/asahanada/Curvable`).

## Glide

Select two or more notes and press **G** (or choose **Glide into legato notes** from the right-click menu). The notes merge into one note that slides from each pitch to the next:

- When there is a gap between two notes, the slide fills the gap: it starts where the first note ends and arrives where the second note starts.
- When the notes touch or overlap, the slide takes up to 1/4 beat and arrives where the next note starts.
- Existing curves, vibrato, volume and pan on the notes are kept in place.

If the selection contains chords, Curvable glides chord to chord, voice by voice, pairing each note with the nearest note of the next chord. When the next chord has fewer notes, voices merge onto the same note; when it has more, the extra notes start on their own. Chords of up to 12 notes are supported.

If a glide needs a wider pitch bend range than the current one, Curvable tells you which range is required.

## Slicing notes

Hold **S** and click a note to cut it at that point, or drag up or down to cut every note the line crosses. You can also choose **Slice here** from a note's right-click menu. Cuts snap to the grid (**Alt** for free placement).

The curve, volume and pan are kept exactly. If you cut a note in the middle of a slide, the second part takes the nearest semitone as its pitch and continues the bend from there, so it still sounds the same. Use this to split a glided note back into separate notes.

## Volume and pan

Every note can have its own volume curve and pan curve, for example to fade one note of a chord out while the others hold, or to move a note from left to right. They work with any synth, because Curvable applies them to the output of the synth copy that plays the note.

Click **VOL** or **PAN** at the left of the bottom lane. Each note's curve is drawn across the note's length:

- **Double-click** in the lane to add a point to the note there, and keep dragging to place it. Points snap to the grid; hold **Alt** for free placement.
- Drag a point to move it. Right-click a point to delete it.
- Drag a note's line anywhere else to raise or lower the whole curve.
- Right-click a note's line for **Fade in**, **Fade out**, **Swell (louder, then softer)** and **Reset volume** (or **Reset pan**).
- When the note is selected, the whole selection changes. A shape you draw is copied to the other selected notes, stretched to each note's length.

Volume ranges from silent to +6 dB, with 0 dB (unchanged) marked by a dashed line. Between two points, the volume moves evenly, like a fader, so a straight line is an even fade. Pan ranges from left (top) to right (bottom). While dragging, the value is shown in dB or as L, C and R with a percentage.

When the note ends, its copy keeps the last volume and pan until it plays its next note, so the release isn't cut off abruptly.

Volume and pan have no effect in MPE mode, where all notes play on one copy of the synth, or on notes that have to share a copy (see [Synth copies](#synth-copies)). They are not included in MIDI export.

## Pitch bend range

**Bend** at the bottom sets how far the pitch wheel bends, in semitones up and down. It must match the pitch bend range set in your synth; otherwise every bend sounds too wide or too narrow. The range is saved with the project.

Choose the smallest range that covers your curves, because pitch wheel resolution is spread across the whole range. Curves beyond the range are clipped when played; Curvable marks them in red and shows a warning.

In [MPE mode](#mpe-mode), Bend must match the synth's **MPE** pitch bend range, which is usually a separate setting from the patch's normal bend range.

## Synth copies

Curvable decides how many copies of the synth a project needs, creates them in the background and removes them a few seconds after they are no longer needed. The synth button shows the count, for example **Serum 2 ×5 (3 on)**:

- **×5** is the number of copies.
- **(3 on)** is the number currently running. A copy pauses when it has no notes and its output has been silent for 3 seconds, and resumes as soon as it gets a note. The main copy always runs, and nothing pauses while rendering.

While a copy is being created, the button shows progress, such as **(2/4)**. Until it is ready, notes assigned to it play without their bend.

Every copy uses the same sound as the main one. Parameter changes are passed on immediately. Changes that don't go through parameters, such as loading a preset in the synth, are copied over shortly afterwards. If a copy ever sounds different, choose **Sync synth copies now** in the settings menu.

Up to 16 notes can bend independently at the same time. A note with its own volume or pan curve counts as bending. Beyond that, notes have to share a copy and follow the newest note's bend while they overlap. Curvable outlines these notes with a red dashed line and shows a warning.

The first time Curvable uses copies, it reminds you to turn off the synth's built-in reverb and delay: each copy would apply them separately.

## MPE mode

For synths that support MPE, turn on **MPE** in the top bar. Curvable then uses a single copy of the synth and sends each bending note on its own MIDI channel (channels 2 to 16, so up to 15 independent bends at once).

1. Turn on MPE in the synth.
2. Turn on **MPE** in Curvable. If Bend is still at its default, Curvable sets it to 48.
3. Make sure **Bend** matches the synth's MPE pitch bend range (often 48). This is usually separate from the patch's normal pitch bend range.

The MPE setting is saved with the project. Volume and pan curves have no effect in MPE mode.

## MIDI import and export

**Import:** drop a `.mid` file onto the piano roll. The notes are added starting at the bar under the mouse and stay selected, so you can move them right away. Pitch bends in the file become curves. Curvable reads the pitch bend range from the file when it is stored there (and uses 48 for MPE files); otherwise it uses the current Bend setting. You can also choose **Add notes from a MIDI file...** from the MIDI button's menu.

**Export:** drag the **MIDI** button into FL Studio (Playlist or Channel rack), another DAW or a folder. Click the button to choose the format used for dragging and to save files:

- **One channel per voice** (default): each voice gets its own MIDI channel and track, with its pitch bends and pitch bend range. Use this to play the result with separate instances of a synth in a host.
- **MPE**: each note takes a member channel (2 to 16) with a pitch bend range of 48. Use this for hosts and synths that support MPE.

Vibrato and trills are written out as pitch bends at the current tempo. Volume and pan curves are not exported. If notes exceed the pitch bend range, or have to share a channel, Curvable tells you after exporting.

## Automation slots

FL Studio can only automate parameters of Curvable itself, not of the synth inside it. Curvable therefore provides 16 automation slots, which FL Studio sees as parameters named **Slot 1** to **Slot 16**.

Open the slots with the **AUTOMATION** handle on the right edge of the Synth page:

1. Click **Learn** on a slot, then move a knob in the synth. The slot is now linked to that knob, and FL Studio shows it as, for example, **1 Cutoff**.
2. Automate the slot in FL Studio as usual. The linked knob follows on every copy of the synth.

Click **×** to unlink a slot. The links are saved with the project.

## Settings and updates

The gear button opens:

- **Theme**: Day or Night.
- **Language**: English, 简体中文, 日本語, 한국어 or Español. The interface switches immediately.
- **Clear all notes**.
- **Sync synth copies now**.
- **Plugin manager...**: scan for plugins and manage the plugin list.
- **Mouse & keyboard shortcuts (F1)**.
- **Space plays FL Studio, not Curvable**.
- **Check for updates now** and **Check for updates automatically**.

These settings apply to all projects. Curvable stores them in its settings folder, together with the plugin list and curve presets.

**Updates.** Curvable checks GitHub for a new release when you open its window, at most once a day. It only reads the public release information and sends nothing else. When a new version is out, a card shows it: **Download** downloads the Windows installer (on macOS, it opens the release page). Close FL Studio before running the installer. **Later** hides the card for that version; **Download Curvable** in the settings menu still offers it. Turn off **Check for updates automatically** to stop the checks.

## Mouse and keyboard reference

| Action | Result |
|---|---|
| Click empty space | Add a note; drag to set its length |
| Drag a note | Move it (time and pitch) |
| Drag a note's right end / left end | Change its length / where it starts |
| Shift + drag a note | Duplicate it |
| Alt + drag | Move or resize without snapping |
| Right-drag | Delete notes |
| Right-click a note | Note menu: vibrato, trill, bends, presets, slice |
| Hold S, click or drag | Cut a note / cut every note the line crosses |
| Drag in the VEL lane, or Alt + wheel on a note | Change velocity |
| Ctrl + drag | Box select |
| Shift + click | Add to or remove from the selection |
| Ctrl + A / Esc | Select all / clear the selection |
| Delete | Delete selected notes |
| Ctrl + C / X / V | Copy / cut / paste at the mouse |
| Ctrl + B or Ctrl + D | Duplicate after the selection |
| Up / Down | Transpose (Shift: octave) |
| Left / Right | Move by the snap step (Shift: bar) |
| Ctrl + Z / Ctrl + Y | Undo / redo |
| Double-click a note | Add a bend point (keep dragging) |
| Drag a point | Move it |
| Drag a small circle | Change the segment's curve shape |
| Right-click a point | Delete it |
| Drag the line above a note up | Add vibrato |
| Drag the amber line | Vibrato depth (trill: the other note) |
| Double-click the amber line | Add a depth point (right-click deletes it) |
| Drag the blue handle | Vibrato speed |
| G | Glide the selected notes or chords |
| Click VOL or PAN | Show each note's volume or pan curve |
| Double-click in the VOL or PAN lane | Add a point |
| Drag a note's line in the lane | Raise or lower the whole curve |
| Right-click in the VOL or PAN lane | Fade in, fade out, swell, reset |
| Space | Play or stop inside Curvable |
| Click / drag / double-click the ruler | Set the start / set the loop / remove the loop |
| Wheel / Shift + wheel | Scroll vertically / horizontally |
| Ctrl + wheel / Ctrl + Shift + wheel | Zoom horizontally / vertically |
| F1 or ? | Show all actions |

## Troubleshooting

**Bends sound too wide or too narrow.** Bend in Curvable and the pitch bend range in the synth differ. Set both to the same value. In MPE mode, compare with the synth's MPE pitch bend range.

**A note follows another note's bend.** More than 16 differently bending notes overlap (the notes are outlined in red), or a new copy of the synth is still being created. Reduce the overlap, or wait a moment after adding bends.

**A volume or pan curve has no effect.** MPE mode is on, or the note has to share a copy with other notes (it is outlined in red).

**Space doesn't start FL Studio.** Space plays inside Curvable while its window has focus. Turn on **Space plays FL Studio, not Curvable** in the settings menu, or click outside Curvable's window first.

**The sound is louder or more reverberant than the synth alone.** Effects inside the synth are applied once per copy. Turn them off in the synth and use the host's effects.

**Copies sound slightly different from each other.** Free-running LFOs or random oscillator phases differ between copies. Enable retrigger in the synth, or choose **Sync synth copies now** after changing presets.

**High CPU use.** Every copy runs a full synth while it has notes. Fewer simultaneous, differently bending notes need fewer copies; idle copies pause automatically. For synths with MPE support, MPE mode needs only one copy.

**A synth doesn't appear in the list.** Scan for it in FL Studio, or in Curvable's **Plugin manager...**, or drag its `.vst3` file onto Curvable.

**"Could not create another copy of the synth".** The synth refused to load another instance, for example because of its license or memory. Curvable retries automatically; meanwhile, the affected notes play without bends.
