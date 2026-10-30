# 008c step 2 — CATCH and LAND: how a breaker lets go

Status: **ready to listen**. Morten's ask: "like somebody trying to push it off the tracks, but it regains
control". Both are in the BRUDD view (right side). Both off by default (CATCH 0 ms, LAND OFF): as step 1.

- **CATCH** (0–1000 ms): let go and the break fights its way back over that time instead of 1.5 ms: the wet
  fades out that slowly, and a tape STOP speeds back up to speed meanwhile (its volume coming back with it).
  700 ms is a Technics SL-1200 MK2 regaining 33 1/3 rpm (its published starting time).
- **LAND** (OFF, 1/16, 1/8, 1/4): let go anywhere; the break holds until the next division, so the return lands
  on the grid: Live's grid while its transport runs, otherwise the grid that started when the pad was pressed.

## The files (CHAPEL, C-E-G, arp UP 1/16 at 90 BPM, 44.1 kHz)

- `before_arp.wav`: no breaker.
- `stop_catch0.wav`, `stop_catch350.wav`, `stop_catch700.wav`: STOP held 4.0–6.0 s. Measured: back in the
  groove at 6.00, 6.34, 6.69 s.
- `half_catch0.wav`, `half_catch700.wav`: HALF held 4.0–6.0 s (HALF fades back slowly; it does not speed up).
- `stutter_released-5.9.wav` vs `stutter_land-beat_released-5.9.wav`: STUTTER let go at 5.9 s, off the beat;
  with LAND 1/4 it holds to the beat at 6.0 s (measured 5.997 s) and comes back on it.
- `stop_land-beat_released-5.9.wav`: STOP let go at 5.9 s with LAND 1/4: back on the beat at 6.0 s.

## What to listen for

1. CATCH 350 / 700 on STOP: does it sound like it regains control, or like a slow fade? Which length?
2. LAND: does landing on the beat make the return feel "straight back into the groove"?
