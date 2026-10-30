# Task 001 — Fork Clipper and record the baseline

**Goal:** Ond exists as a fork of Clipper, builds its host renderer, and has a set of baseline
renders of the sounds Morten loves. No sound changes in this task.

## Steps

1. The repo is a fork of `isod89/clipper-fm1` with `upstream` pointing to it (see README.md for
   the commands Morten ran). Confirm `git remote -v` shows both `origin` and `upstream`.
2. Read `tests/run_tests.sh` and build only what the host renderer needs: the generated tables
   in `build/gen/` and `build/host/hostsim` (see its header comment in `tests/hostsim.c` for
   arguments and env variables). Do not change hostsim's behaviour for existing arguments.
3. Find the engine index and preset index of these presets (engine list in
   `firmware/src/engines.c`, presets in each `eng_*.c` / `hybrid_patches.h`):
   - WHEEL (drawbar): CHAPEL, REED ORGAN, SOFT FLUTE, STRING ORG
   - Drones: DRONE (HYBRID), DARK DRONE and DRONE (GRAIN), SPACE DRONE (VECTOR), FOLD DRONE (TERRAIN)
4. If hostsim cannot set DUST (`G_DUST`), add an env variable `DUST=0..127` to hostsim only
   (test harness, not DSP). Same for the reverb type if it cannot be set.
5. Write `tools/render_refs.sh` that renders, for every preset above, with `PRESET=1`, a held
   3-note chord (`CHORD=3`), 12 seconds, into `refs/baseline/`:
   - `<preset>_dust0.wav`, `<preset>_dust40.wav`, `<preset>_dust90.wav`
   File names lowercase, spaces as `-`.
6. Write `refs/baseline/LISTEN.md`: a table of the files, and for each preset one line on what
   it is. Morten will compare these against the emulator in his browser.
7. Commit: "baseline: host renderer and reference renders".

## Done when
- `tools/render_refs.sh` runs from a clean checkout with one command and regenerates everything.
- Morten has listened and confirms the renders sound like the emulator. Until then: "ready to
  listen".

## Not in this task
- Building the FM-1 firmware package (needs the JieLi toolchain): task 002.
- Any DSP change.
