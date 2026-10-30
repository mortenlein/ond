# Task 008c — Brudd, stage 1: the breakers

**Goal:** the eight breakers of `docs/rhythm-breaks.md` (bank B): held on a pad, they act on whatever Ond is
playing, and let go when the pad does. A sound task: each breaker is its own change, rendered next to the
baseline, and Morten decides by ear before the next one starts.

## Where they come from (no new sound invented where one exists)

| # | Breaker | Source |
| --- | --- | --- |
| 1 | Stutter: loop the last 16th | DUB-1 (github.com/sbegueria/fm1-dub, GPL-3.0) `amb.c`: stutter |
| 2 | Tape stop | DUB-1 `amb.c`: tape stop |
| 3 | Freeze | DUB-1 `amb.c`: freeze |
| 4 | Half speed | DUB-1's tape read at half rate |
| 5 | Crush | Clipper DUST (`fx.c dust_process`), pushed while held |
| 6 | Duck: the pump | Clipper's DUCK (`fx.c duck_block`, the groovebox's; off in the synth) |
| 7 | Dropout | new: a mute with a ramp (no tone of its own); last, Morten may drop it |
| 8 | Reverse | new: the last beat read backwards; last, Morten may drop it |

## Slip: it always regains control (Morten, 2026-10-30)

Morten: "straight back into the previous groove when used, like somebody trying to push it off the tracks, but
it regains control."

- A breaker acts on a **copy** of the output. The instrument, the arp and the clock never stop underneath, so
  letting go hands back to the groove **where it is now**, in time (as a DJ player's slip mode).
- **CATCH**: how it fights its way back on release: the broken sound speeds back up and crossfades into the live
  groove. From instant to a turntable regaining speed: Technics SL-1200 MK2, 0.7 s to reach 33 1/3 rpm
  (its published starting characteristics). Morten sets it by ear.
- **LAND**: the return lands on the grid: OFF (at release), 1/16, 1/8, a beat. The breaker holds until then.
- Both are part of every breaker from the first one on (stutter), not added later.

## Steps

1. Read DUB-1's `amb.c` (stutter, tape stop, freeze, the tape's rate and buffer) and note in
   `docs/sound-lessons.md` what each does and its values.
2. `firmware/src/brudd.c`: one stage on the master, after the mix and before DUST, portable C as the code around
   it (Q15, per-block control, ramps, no allocation). A breaker's buffer sized by a define: the FM-1's RAM is
   nearly full (`pool 313528 of 344064`), so the FM-1 build may get shorter buffers or fewer breakers; say
   what each costs.
3. Played from: MIDI notes on channel 10 (the MPK's pads, `docs/controller-mpk-mini.md`), velocity = depth;
   and on the plugin's screen, eight pads held with the mouse.
4. One breaker at a time, in the order above: renders `refs/008c-<n>/before_*.wav` / `after_*.wav` (a sound
   held, the breaker pressed and let go in time), `LISTEN.md`. Ready to listen; Morten marks done.

## Not in this task
- The hits (bank A, samples: stage 2) and the pad loop (stage 3).

## Step 1 (2026-10-30): ready to listen — `refs/008c-1/LISTEN.md`

Found in this repo, better than porting DUB-1: SLOOP's punch-in FX (`punch.c`) are these breakers already,
integer, ear-tuned, beat-synced, slipping back to the live mix on release; Clipper had them off only for the
FM-1's memory. `OND_PUNCH` (off by default) turns them on for Ond's desktop engine; the FM-1 firmware is the
same file as before. Played from channel 10 (MPK bank B, notes 44–51), the screen's BRUDD pads, or the
"Breaker" parameter (automation). DUB-1's stutter / tape stop / freeze values are in `docs/sound-lessons.md` §8
for when Morten wants their character instead. Next, after Morten's verdict: CATCH, LAND, transport sync,
velocity, Duck.
