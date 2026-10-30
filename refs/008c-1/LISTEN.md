# 008c step 1 — Brudd's breakers: SLOOP's punch-in effects in Ond

Status: **ready to listen**. Morten decides which breakers stay, and on which pads.

The breakers are SLOOP's punch-in FX (`firmware/src/punch.c`, tuned by ear on the hardware), switched back on
for Ond's desktop builds. Nothing else changed: with no breaker held the plugin plays the baseline's samples, and
the FM-1 firmware is the same file as before (byte for byte).

## The files (44.1 kHz, 12 s, through the plugin)

- `before_arp.wav`: CHAPEL, C-E-G held 0.1–9 s, arp UP 1/16 at 90 BPM. No breaker.
- `after_arp_<breaker>.wav`: the same, the breaker **held 4.0–6.0 s**, then **tapped 7.0–7.25 s**. All 16.
- `before_drone.wav`, `after_drone_{stop,half,reverse,loop-4}.wav`: SPACE DRONE, the tape-like four, same timing.

## What to listen for

1. **Slip** (the main thing): after you let go (6.0 s, 7.25 s), is the arp straight back in the groove, as if
   nothing happened? (Measured: from 0.1 s after release the output is the same as `before_*`, within 0.002.)
   Does the return feel like "regaining control", or too abrupt? It is a 1.5 ms crossfade now; CATCH (a
   turntable-like recovery) and LAND (return on the grid) come next if you want them.
2. **Each breaker:** keep, drop, or wants changing. My guesses for your roles (the MPK's bank B, marked
   PAD 1..8 on the screen): 1 STUTTER, 2 STOP (tape stop), 3 GATE (dropout), 4 CRUSH, 5 HALF, 6 REVERSE,
   7 LOOP 4 (freeze: a beat held), 8 ECHO. Duck (the pump) is not among them yet.
3. **The tap** at 7.0 s: a quarter-second hit of each, the rhythmic use.

## Notes

- Without a running transport (here, and in Live with the transport stopped) the loops start where you press;
  with it they snap to the grid. Synced to Live's transport is a next step.
- Velocity does nothing yet (the punch-in effects have no depth).
