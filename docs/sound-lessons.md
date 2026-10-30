# Sound lessons from the FM-1 firmwares

Why these firmwares sound good to a human. Read before changing any sound. Paths are in the
upstream repos (clipper-fm1, fm1-fomni, fm1-dub), all GPL-3.0.

## 1. Nothing starts identical
- `clipper-fm1/firmware/src/eng_drawbar.c`: every partial has a free-running phase. A note starts
  wherever its "wheels" are, as on a real tonewheel generator. Start phases are spread once with
  golden-ratio hashing per partial and voice.
- Lesson: never reset phases on note-on for sustained voices.

## 2. Values come from real instruments, not formulas
- `fm1-fomni/firmware/src/dsp/omni.c`: the shimmer rate depends on the chord root, measured from a
  real Omnichord: C 3.761, C# 2.910, D 4.275, D# 4.565, E 5.050, F 6.333, F# 2.843, G 3.125,
  G# 3.162, A 3.419, A# 3.667, B 3.953 Hz. Higher strings shimmer less; each string its own phase.
  Brightness darkens as the note decays. Shimmer shape |sin|^0.6.
- `clipper-fm1/firmware/src/fx.c`, Juno chorus: from published measurements of the Juno-60/106:
  mode I 0.513 Hz, 1.66–5.35 ms; II 0.863 Hz; I+II 9.75 Hz, 3.3–3.7 ms; taps low-passed as a BBD.
- `eng_drawbar.c` registrations are named, played settings (CHAPL 004432000, REED 008765432…).

## 3. Imperfection is mechanism
- Key click: short high-passed noise at key contact (~2.5 ms), half as loud at release.
- Percussion: single trigger only (legato playing gets none), and the bars drop ~3 dB, as on the
  real circuit.
- Rotary: two rotors with different inertia (high ~0.37 s, low ~1.5 s to change speed), AM and
  Doppler depth follow speed. No delay line: folded into per-block gains.

## 4. Lo-fi by actually being lo-fi
- `fm1-dub/firmware/src/dsp/amb.c`, tape echo: the tape runs at a quarter rate (11.025 kHz,
  16-bit, mono, up to 5.6 s). Each repeat goes through a band-pass and five allpasses (3–40 ms),
  so it comes back softer, darker and smeared. A soft clip in the feedback lets it pass 100 %
  and swell without running away. Wow = slow modulation of the read position. The second head
  wanders; tape stop slows the read; freeze = the loop stops listening; stutter loops a 16th.
  The first repeat is kept clean ("the classics pass"); blur grows with each pass.
- `fx.c` DUST: one knob turns everything up together: drive into a soft clip, sample-and-hold
  (44.1 → 11 kHz), fewer bits (15 → 8), one-pole low-pass (open → ~3 kHz), hiss and crackle.
  Hiss and crackle fade in with PLAY and out at STOP (~0.1 s): silence stays silent.

## 5. Chords do not stack into clipping
- `eng_drawbar.c`: soft normalisation, sum of gains = 2.4 s / (s + 1.5): one bar 0.96, three
  1.6, all nine 2.06. Fuller, not harder.
- Partials above ~6 kHz fold down an octave, as the top of a tonewheel set does: no aliasing.

## 6. No zipper, no clicks
- Everything that moves is computed once per block (32 samples) and ramped across the block.
- Changes that cannot be ramped fade the part out (~3–6 ms) and back in.

## 7. How they work
- They render on a computer (`clipper-fm1/tests/hostsim.c`) and listen, and they compare against
  classic records in written reports ("the classics pass"). Measurement plus ears, every step.

## Gaps (ours to build)
- No firmware has a real free-reed harmonium voice; Clipper's REED is drawbar sines.
- No bellows. No bad-cable stage. No prism harmoniser.
