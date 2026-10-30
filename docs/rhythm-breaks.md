# Brudd — rhythm from broken sounds

Morten's idea: play a rhythm on the MPK pads made of broken sounds and things that break the
music up. Very lo-fi. References as in vision.md, plus: Nils Frahm's piano mechanics as
percussion (felt, hammers, pedals), the chopped glitches of *22, A Million*, lo-fi beats with a
dusty, ducking pulse.

The rule: the percussion comes from the instrument itself and from its wear, not from a drum
machine. A harmonium has no drums, but it has a bellows, keys, stops, wood and air.

## Source rule (Morten, 2026-10-30)

Morten: "Claude and sounds are never good enough." So:

- **No hit is synthesised by Claude.** Bank A plays **recorded samples**: Morten's own recordings
  (phone or interface: bellows, wood, keys, felt, pedals, cables, room) and CC0 recordings from
  VCSL / VSCO-2-CE (github.com/sgossner, already used by Felucca's tools/fetch_cc0.py). Claude's
  job is the sampler plumbing (bring back SLOOP's user sample slots, which Clipper removed), not
  the sounds.
- **Breakers reuse existing, ear-tuned code** wherever it exists (DUB-1, Clipper DUST/DUCK).
  The two without a source (dropout, reverse) are plain buffer operations with no tone of their
  own, and come last; Morten can drop them.
- The table below names a *role* for each pad. The sound in it is whatever recording Morten picks.

## Two kinds of pads

**Bank A — hits (one-shots, velocity sensitive)**

| Pad | Role | Record / find |
| --- | --- | --- |
| 1 | Low thump, the "kick" | a bellows, a door, a hand on a wooden box, a soft bass drum |
| 2 | Felt, muffled kick | a felt piano hammer, a palm on a cushioned surface |
| 3 | Wood knock | a drawer, a stop, a chair, a woodblock (VCSL) |
| 4 | Mechanical click | a key, a switch, a pen, a camera shutter |
| 5 | Crackle | a record run-out groove, crumpled paper, fire |
| 6 | Hum bump | a guitar cable plugged into a live amp |
| 7 | Tape start | a real cassette or reel starting (or a turntable spinning up) |
| 8 | Air / snare role | breath, a brush on paper, a shaker (VCSL) |

**Bank B — breakers (act on what is playing, held as long as the pad is)**

| Pad | Action | From |
| --- | --- | --- |
| 1 | Stutter — loop the last 16th | existing: DUB-1 stutter |
| 2 | Tape stop — slow the whole sound to a halt | existing: DUB-1 tape stop |
| 3 | Dropout — the cable cuts the sound out | new, plain mute with a ramp (last) |
| 4 | Crush — bits and rate drop while held | existing: Clipper DUST, pushed |
| 5 | Half speed — an octave down, slower | adapted: DUB-1 tape read at half rate |
| 6 | Reverse — the last beat played backwards | new, plain buffer read (last) |
| 7 | Freeze — the sound held, stops listening | existing: DUB-1 freeze |
| 8 | Duck — the pump: the organ dips and swells back | existing: Clipper DUCK |

Velocity: a hit's level and darkness; a breaker's depth.

## How it sits in the mix

- Hits go through their own short DUST + tape bus: always dusty, even when the organ is clean.
- The low thump and felt pads duck the organ (Clipper's DUCK), giving the lo-fi pump.
- Hits are slightly late and uneven by default (a few ms, random), never on a perfect grid.
  Morten tunes how much by ear.

## Playing it

- MPK **NOTE REPEAT** + its tempo, on a hit pad: rolls; on a breaker: rhythmic stutters.
- MPK **FULL LEVEL** off: velocity matters.
- MPK **PROG CHANGE** button: pads temporarily load sounds instead (favourite sounds stay one
  button away, so the pads can be notes by default).
- The pad loop (below).

## The pad loop

Morten: "a 4 or 8 bar mode that repeats."

- **Length:** 4 or 8 bars, chosen before recording. 4/4.
- **Tempo:** Ond's tempo (GLO), or MIDI clock when Ableton runs the show. A one-bar count-in
  (a dusty click) before recording starts.
- **Recording:** overdub. Every pass, new pad hits are added on top of what is there. Both hits
  and breakers are recorded, so a tape stop at the end of bar 8 comes back every time.
- **Timing:** what Morten plays is kept as played (Clipper's REC QUANT OFF keeps timing as nudges),
  with an optional light pull towards the grid. Morten sets how much.
- **Wear over time:** each pass the loop ages a little: tiny timing drift and a touch more dust,
  like a worn tape loop. A knob from "the same every time" to "falls apart slowly". Never drifts
  out of tempo with the organ or with Ableton.
- **Controls:** record/overdub, play/stop, undo last pass, erase one pad (hold it + erase), clear.
- **Reuse first:** check if Clipper's step sequencer and recorder can hold 8 bars of pad events;
  extend it if not, do not write a new one.

## Build order

1. Breakers first (they reuse DUB-1 and DUST code that already sounds right).
2. Then the sampler for the hits (sample slots, velocity, load from the web editor). Morten records
   and chooses the sounds.
3. Then the pad loop: first plain 4/8-bar overdub; then undo/erase; then the wear over time.
