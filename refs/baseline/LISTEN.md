# Baseline renders — Clipper as it is (task 001)

Status: **ready to listen**. Nothing in the sound has been changed. These are the reference that every
later change is compared against.

Regenerate with `sh tools/render_refs.sh` from the repo root (on Windows it runs itself in WSL).

## How they were made

- Clipper's own DSP sources, built for the host as the FM-1 runs them (`CLIPPER_SYNTH=1`).
- Each preset loaded whole (`PRESET=1`: sound, chorus send, voice mode, spread).
- **Delay and reverb sends at 0**, as the FM-1 loads a factory preset (Clipper 1.0: "the factory sounds
  dry"). So to compare with the emulator: load the preset and play, without turning up DLY / REV.
  Reverb TYPE is the default (ROOM) but is not heard at send 0.
- A held 3-note chord, C4 E4 G4 (the keys as at octave 0), pressed at 0.1 s, released at 9 s; 12 s in all,
  so the last 3 s are the release. Long drone releases are still sounding when the file ends at 12 s.
- DUST at 0, 40 and 90 (of 127, the GLO → MASTER DUST knob; 40 ≈ 31 %, 90 ≈ 71 % on the screen).
  The transport is stopped, as when you just play the keys: DUST's hiss and crackle only run with PLAY,
  so here you hear the drive, the lower sample rate, fewer bits and the low-pass, not the record noise.
- 44.1 kHz, 16-bit stereo. The master limiter holds peaks at about −5 dBFS; nothing clips.

## Files

| Preset | Engine (index) / preset index | DUST 0 | DUST 40 | DUST 90 |
|---|---|---|---|---|
| CHAPEL | WHEEL (7) / 6 | `chapel_dust0.wav` | `chapel_dust40.wav` | `chapel_dust90.wav` |
| REED ORGAN | WHEEL (7) / 13 | `reed-organ_dust0.wav` | `reed-organ_dust40.wav` | `reed-organ_dust90.wav` |
| SOFT FLUTE | WHEEL (7) / 10 | `soft-flute_dust0.wav` | `soft-flute_dust40.wav` | `soft-flute_dust90.wav` |
| STRING ORG | WHEEL (7) / 12 | `string-org_dust0.wav` | `string-org_dust40.wav` | `string-org_dust90.wav` |
| DRONE | HYBRID (12) / 18 | `drone_dust0.wav` | `drone_dust40.wav` | `drone_dust90.wav` |
| DARK DRONE | GRAIN (8) / 6 | `dark-drone_dust0.wav` | `dark-drone_dust40.wav` | `dark-drone_dust90.wav` |
| SPACE DRONE | VECTOR (22) / 13 | `space-drone_dust0.wav` | `space-drone_dust40.wav` | `space-drone_dust90.wav` |
| FOLD DRONE | TERRAIN (18) / 5 | `fold-drone_dust0.wav` | `fold-drone_dust40.wav` | `fold-drone_dust90.wav` |

## What each preset is

- **CHAPEL** — drawbar organ, registration CHAPL (004432000: soft, churchy), slow rotor, a little key click,
  short swell in.
- **REED ORGAN** — drawbar organ, registration REED (008765432: a falling slope, brassy), some drive, no rotor,
  a little chorus. Note: drawbar sines, not a real free reed.
- **SOFT FLUTE** — drawbar organ, registration FLUTE (008000000: one soft 8' flute), slow rotor.
- **STRING ORG** — drawbar organ, registration STRNG (006876540: bright and thin), slow rotor, chorus.
- **DRONE** (HYBRID) — three oscillators on a fifth and an octave (super-saw, saw, pulse an octave down),
  slow filters, heavy drift, LFOs moving cutoff and filter balance.
- **DARK DRONE** (GRAIN) — a grain cloud of Clipper's own PAD source (seven saws), an octave down, dark tone.
- **SPACE DRONE** (VECTOR) — four wavetable corners mixed by a random vector movement, very slow attack and
  long release, strong chorus.
- **FOLD DRONE** (TERRAIN) — wave terrain synthesis on a folding surface, wide orbit with drift.

## What to listen for (against the emulator)

1. Does each `_dust0` file sound like the same preset played in the emulator (same chord, no DLY / REV)?
   Tone, level, rotor / movement, attack and release.
2. Do `_dust40` and `_dust90` sound like DUST turned to those values in the emulator?
3. Anything that sounds wrong or different here is a problem with the renderer, not with Clipper: tell me
   and I'll fix the render before any sound work starts.

## Notes

- The task lists "DRONE (GRAIN)". In Clipper, GRAIN has only DARK DRONE; GRAIN's "DRONE" is a SLOOP
  groovebox preset (built from the CC0 horn samples) that is not in the FM-1 firmware or the emulator, so it
  is not rendered.
