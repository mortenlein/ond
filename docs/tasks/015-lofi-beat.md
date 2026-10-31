# Task 015 — BEAT: a simple lo-fi beat that makes itself

**Goal (Morten, 2026-10-31):** "a simple lofi beat part of the plugin, I am really bad at making them so any help
it can give for free is amazing." Press play in Live, pick a feel, and a dusty, swung, slightly sloppy beat plays
under Ond, in time with Live, changing a little every bar. One button for a new one. Out as MIDI to edit in Live
when wanted.

## The rules it keeps

- **No sound made by Claude** (`docs/rhythm-breaks.md`). The drums are: SLOOP's own kits (existing, ear-tuned code by
  Clipper's/SLOOP's authors: LO-FI, BOOMBAP, JAZZ, AMBIENT, 808, VINTAGE…, and the sampled CC0 kit with its DUSTY
  treatment), and Morten's own recordings (the Hits folder as a kit). Morten picks.
- **No groove invented by Claude.** The patterns and their human timing come from real drummers (below), or from
  named, documented idioms. Swing is SLOOP's MPC swing (50 % straight .. 75 %, as the MPC measures it).
- **Reuse first:** SLOOP's drum machine is still in the code (`drums.c`, `drum_synth.c`: lanes, voices, chokes;
  `seq.c`: the drum track's steps, swing, rolls, ratchets, SPICE / DICE variations). Clipper only left it out of
  the FM-1's image to fit the flash. Ond's desktop engine turns it back on (as `OND_PUNCH` did for the breakers);
  the FM-1 build stays as it is.
- **Small steps, Morten's ears:** each stage its own change with renders.

## Where the grooves come from

**Proposed: the Groove MIDI Dataset** (Google Magenta, CC BY 4.0, magenta.tensorflow.org/datasets/groove): 13.6
hours of 10 drummers on an electronic kit, as MIDI, labelled by style and tempo, with their real timing (how far
each hit sits off the grid) and velocities. Its hip-hop, funk, soul, jazz and "neo-soul"-labelled beats at 70–100
BPM are lo-fi's raw material. A tool (`tools/gen_grooves.py`) picks the one- and two-bar grooves that loop cleanly,
keeps each hit's offset from the 16th and its velocity, and maps the kit's notes onto SLOOP's lanes (kick, snare,
closed / open hat, rim, perc …). A few dozen grooves, grouped by feel. Credited in `CREDITS.txt` (CC BY asks for
attribution; data, not code: no change to Ond's GPL).

Alternative if Morten prefers: a handful of named idioms written by hand from published drum notation (boom bap,
half-time, brushes), with timing from the dataset only.

## How it plays

- **FEEL** (the groove family): e.g. LAZY (laid back, late snares), BOOM BAP, HALF-TIME, BRUSHES (jazz), SPARSE
  (kick and rim only), OFF.
- **NEW**: another groove of the same feel. **KEEP**: this one stays.
- **Every bar a little different**: SLOOP's SPICE / DICE vary the steps (a ghost note more or less, a hat dropped);
  every 4th or 8th bar a fill from the same drummer's playing.
- **DRUNK**: how much of the drummer's own timing to keep (0: on the grid, 100 %: as played, more: looser still).
- **SWING**: SLOOP's (MPC) swing over it.
- **KIT**: SLOOP's kits, or YOURS (the Hits folder, a recording per lane).
- **The lo-fi glue** (each a sound change for Morten's ear, one at a time):
  - the beat's own bus through DUST (dusty even when the organ is clean, `docs/rhythm-breaks.md`);
  - **PUMP**: the kick ducks the organ (Clipper's own DUCK, `fx.c duck_block`, the groovebox's, off in the synth);
  - the beat through the breakers too (a tape STOP takes the beat with it).
- **Plays while Live plays** (Ond follows Live's transport, task 014): bar 1 of Live is bar 1 of the beat. In the
  standalone, PLAY on its screen.
- **Out as MIDI**: SAVE MIDI writes the beat (8 bars, its timing and velocities) to `Documents\Ond\Beats\*.mid`, to
  drag into a Live clip (onto Ond's own notes, or a Drum Rack), to edit by hand.

## Steps

1. The drum machine on in Ond's desktop engine, run by Live's transport; one fixed groove; renders. (`OND_BEAT`,
   FM-1 unchanged.)
2. The grooves: the dataset (or the idioms) → `build/gen/ond_grooves.h`; FEEL, NEW, KEEP.
3. Every bar different: SPICE / DICE on the drum track; fills.
4. DRUNK and SWING.
5. KIT: SLOOP's kits, and YOURS (the Hits folder per lane).
6. The glue, one at a time: the beat's DUST bus, PUMP, through the breakers.
7. The BEAT view on the screen (like BRUDD / LAYER).
8. SAVE MIDI.

## Morten decided (2026-10-31)

1. Grooves: **the Groove MIDI Dataset** (real drummers). 2. Sounds: **SLOOP's kits and recordings**. 3. Output: **both**
   (inside Ond, and SAVE MIDI).

## The questions were

1. Grooves from the **Groove MIDI Dataset** (real drummers, CC BY 4.0), or **hand-written idioms**?
2. Drum sounds: **SLOOP's kits allowed** (existing code by SLOOP's authors, not Claude's), or **only recordings**
   (the CC0 kit and your Hits)?
3. The beat **inside Ond** (its own drums, through Ond's DUST and breakers), **MIDI out** to Live (a Drum Rack of
   your choice), or **both** (as planned)?

## Done when
- Morten presses play in Live, picks a feel, and likes the beat under Ond without having programmed a step.

## Step 1 (2026-10-31): the drum machine on, on Live's grid — `refs/015-1/`

`OND_BEAT` (core.h, 0 for the FM-1: its build is the same file) opens SLOOP's drum machine in Ond's desktop
engine at its five gates: the render (`fx.c`), the drum track's steps (`seq.c`), its note-on (`voice.c`), its
"only track 1 sounds" (`core.h trk_silent`), and its level and sends (`drums.c DR_*`: Clipper's slots of those
are the master EQ). Controls: FEEL (OFF / TEST), KIT (SLOOP's 32), LEVEL. hostsim's baseline byte-identical.

## Step 2 (2026-10-31): the grooves — `refs/015-2/`

`tools/gen_grooves.py` → `host/ond_grooves.h` (committed): 76 two-bar grooves from the Groove MIDI Dataset's 4/4
beats at 65–100 BPM (HIP HOP 26, FUNK 32, SOUL 11, JAZZ 7), each the two bars that repeat best (most 90–100 %).
FEEL / GROOVE / DRUNK in the engine (DRUNK, step 4's, came with it: it is how the timing is applied). Credited in
`LICENSING.md`. The dataset itself is not in the repo (`C:\dev
epos
eference\groove`, CC BY 4.0).

## Step 3 (2026-10-31): every bar different, fills — `refs/015-3/`

Not SPICE (it skips whole steps: kick and snare too). The drummers' own playing instead: up to 8 bars from where
the groove locks, and a fill bar from the fill recordings (`tools/gen_grooves.py`). The drum track holds two bars;
the next is written into the half not playing, a bar ahead (`beat_schedule`). BARS (2 / 8, default 8), FILL
(OFF / 4 / 8, default 8).

## Parts in / out (2026-10-31, Ond 0.12; Morten: "toggle things out of the loop, hihat or cymbals")

KICK, SNARE (snare, clap, snare 2), RIM, HATS (closed, open, pedal), CYMBALS (crash, ride), TOMS, PERC (shaker,
conga, cowbell): each a parameter (Live can automate it), IN by default, a row of switches in the BEAT view. A part
out leaves its lanes empty from the next step (both bars written again); MIDI out sends only what plays. Checked
through the plugin's MIDI out: HATS out removes the 45 hats and nothing else; out at 4 s, back at 8 s.
