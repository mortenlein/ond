# Task 013 — Desktop: a proper screen and more to play with

**Goal:** Ond in Ableton gets its own window, and Clipper's own controls that the plugin does not show yet. No
change to the sound engine: every control here is one the FM-1 already has (values, ranges, names and value
text from the firmware's tables and `param_format`). New sound stages (bellows, tape, …) stay their own tasks.

## Steps

1. **Controls.** `host/ond_engine`: a table of the controls the plugin shows (stable keys, append-only: Live sets
   keep them), with the firmware's ranges, names (the engine's own for EDIT 1..8) and value text. A factory
   preset's values without loading it, so the screen can follow a sound change. Host tempo → BPM (arp, delay).
   The controls:
   - SOUND; MACROS: BRITE, TIMBR, MOTN, SPACE;
   - ENGINE: EDIT 1..8 (named per engine); ENVELOPE: ATTACK, DECAY, SUSTAIN, RELEASE;
   - CHARACTER: VINTAGE, SPREAD, GLIDE, VOICE (poly / mono / legato / unison), DETUNE, LFO rate / wave / to
     pitch / to filter;
   - PLAY: HOLD (latch), ARP mode / rate / octaves / gate / probability, SPICE. (CHORD and STRUM work only from
     the FM-1's own keys, not over MIDI: left for task 009, harmonising);
   - SPACE: DUST, FILTER (the master DJ filter), CHORUS (send, rate, depth), DELAY (send, time, feedback,
     mode), REVERB (send, type, size, damping), LEVEL.
   Kept across sound changes (the player's, not the preset's): the SPACE section's sends and LEVEL.
2. **Screen** (DPF / NanoVG, OpenGL): the sound browser (the baseline eight on top, every engine's presets,
   names), the four macros large, the sections above as knobs with the FM-1's value text, the name drawn
   from the FM-1's own pixel font. Dark, warm, one accent.
3. **Standalone** `Ond.exe` (DPF's native audio fallback) so Ond plays without Ableton too, and so the screen
   can be checked here.

## Done when
- Morten has played it in Live and likes the screen; the sound is unchanged (renders as task 011's).

## Status: ready to play in Ableton (steps 1–2; 3, the standalone, next)

- 45 controls, all the FM-1's, plus SOUND (all 399 factory sounds but SAMPLE's) and an output meter. 0.1's seven
  parameters keep their ids.
- The screen: `plugin/OndUI.cpp`. Checked with `plugin/clap_gui_shot.c` (opens the plugin's screen on Windows,
  saves it, can click in it).
- Sound unchanged: the eight baseline sounds through the 0.2 plugin, DUST 0 / 40 / 90, the same samples as
  `refs/baseline/` (within 1 LSB). A sound set from the host alone loads whole; only what the host or the
  screen set goes over it.
- Every control reaches the engine (renders differ), except BRITE / TIMBR on WHEEL: the drawbar organ has no
  cutoff or shape for them (the FM-1 is the same).

## 0.3: the player's presets

- SAVE AS: the sound and every control as a preset, named after the sound ("CHAPEL 2"); type to rename it
  (Enter). SAVE: over the preset loaded (lit once something changed; the name shows `*`).
- Files: `Documents/Ond/Presets/<name>.ondpreset`, text: the sound by its engine's and preset's names, every
  control by its key. `OND_PRESET_DIR` points elsewhere (tests).
- The browser: MINE first (rename, delete with a second click, open the folder). `<` `>` step through my
  presets while one is loaded. The Live set keeps which preset was loaded (plugin state `preset`).
- Checked with `clap_gui_shot` (save as, type a name, change, save, browse, delete asked, load in a fresh
  instance) in a test folder. Sound unchanged (baseline renders, within 1 LSB).

## Step 3 (2026-10-31): the standalone `build/plugin/bin/Ond.exe`

DPF's native fallback (RtAudio over WASAPI, RtMidi over Windows MIDI), one static exe, Windows' own DLLs only.
Built, not yet started here (it opens audio on Morten's machine): Morten tries it — its settings (audio device,
MIDI in) are in DPF's standalone menu.
