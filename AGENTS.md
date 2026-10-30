# AGENTS.md — Ond

Ond is a fork of Clipper (isod89/clipper-fm1, itself built on Felucca by Leo Kuroshita / Hügelton
Instruments). It becomes a drone-organ / harmonium instrument with tape wear, for the M-VAVE FM-1,
the desktop (Ableton) and the phone. Read `docs/vision.md` and `docs/sound-lessons.md` before any work.

## The one rule

**Morten decides every sound by ear. You never do.**
Clipper as it is today is the reference Morten loves. Nothing you change may make the sound worse
than that reference. You cannot hear, so you never judge a sound as "better" or "done" yourself:
you render it, put it next to the baseline, and hand both to Morten.

## How work happens

1. Work only on the task in `docs/tasks/` that Morten points you to. One task at a time.
2. Every change that touches sound ends with renders: the same notes through the baseline and
   through the change, written to `refs/<task-id>/` as `before_*.wav` and `after_*.wav`, plus a
   short `refs/<task-id>/LISTEN.md` saying what to listen for in each pair.
3. Do not mark a sound task done. Mark it "ready to listen". Morten marks it done.
4. If Morten says something sounds wrong, revert to the last version he approved before trying
   anything new. Do not stack fixes on a rejected sound.

## How to change sound

- **Extend, don't rewrite.** Clipper's engines and effects were tuned by people listening on
  hardware. Reuse them. Never replace an existing engine or effect with a "cleaner" version.
- **Copy the instrument, not the math.** New parts model a real mechanism (a bellows, a reed, a
  tape head) with values taken from measurements of real instruments or from existing firmware
  code, written down in `docs/sound-lessons.md` with their source. No invented constants without
  a note saying so.
- **Nothing starts identical.** Free-running phases, per-voice offsets, irregular rates.
- **Small steps.** One new parameter or one new stage per change, so a bad change is easy to hear
  and easy to undo.

## Code rules

- Portable C, the style of the surrounding Clipper code (integer / Q15 where the code around it is,
  per-block control, gain ramps, no allocation in the audio path).
- The same DSP source must build for the FM-1, the host renderer (`tests/hostsim.c`) and later the
  plugin and web builds. No platform code inside DSP files.
- Watch the FM-1 budget: Clipper reports CPU in GLO → SYSTEM; keep new stages cheap and say what
  they cost.
- License: GPL-3.0-only, as upstream. Keep upstream copyright headers. New files get the same SPDX
  header with Morten Lein as copyright holder.
- Keep `upstream` as a git remote so Clipper fixes can be merged in.

## Commits

Small commits, one change each, message says what changed in sound terms ("bellows: slow breath
on volume only"), not only code terms.
