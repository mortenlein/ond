# Akai MPK Mini mk2 → Ond

Morten's controller: 25 keys, 8 knobs, 16 pads (banks A/B), joystick, sustain pedal input.
Stored on the MPK as **PROG 4 "Ond"** (PROG 1 stays the factory default).

## Stage 1 — today, Clipper's own MIDI (docs/midi.md)

| Control | Sends | Clipper does |
| --- | --- | --- |
| K1 | CC 16 | BRITE (cutoff ±2.5 oct) |
| K2 | CC 17 | TIMBR (the engine's shape) |
| K3 | CC 18 | MOTN (vibrato + filter motion) |
| K4 | CC 19 | SPACE (more reverb, delay, spread) |
| K5 | CC 11 | Expression (volume) |
| K6 | CC 91 | Reverb send |
| K7 | CC 93 | Chorus send |
| K8 | CC 94 | Delay send |
| Joystick X | Pitch bend | Pitch bend |
| Joystick Y up | CC 1 | Mod wheel |
| Sustain pedal | CC 64 | Hold (drones keep ringing) |
| Pads (PROG CHANGE button on) | Program change | Load 8 favourite sounds (program number = position in Clipper's sound list; bank via CC 0 if > 127) |
| Pads bank A 1–8 | Notes on ch. 10 | Brudd hits (docs/rhythm-breaks.md, stage 2) |
| Pads bank B 1–8 | Notes on ch. 10 | Brudd breakers (docs/rhythm-breaks.md, stage 2) |

Experiment (bellows today): GLO > MIDI 2 > EWI = VOL makes CC 2 (breath) the volume. Set joystick
Y up to CC 2 instead of CC 1: no push, no air, no sound; push to pump. Morten decides if it stays.

## Stage 2 — Ond's own CCs (backlog 008b)

Clipper has no MIDI learn and no CC for DUST. Ond adds a fixed CC map for its new controls,
chosen so stage 1's knobs keep working:

| CC | Ond control |
| --- | --- |
| 2 | Bellows pressure (breath) |
| 21 | DUST |
| 22 | WEAR macro (DUST + tape + cable) |
| 23 | Tape wow / flutter |
| 24 | Tape echo amount |
| 25 | Drone layer level |
| 26 | Harmoniser mode / spread |

Pads: notes on channel 10 (Brudd, docs/rhythm-breaks.md): bank A hits, bank B breakers. Ond's
MIDI IN must not let channel 10 play the organ. Drone on/off and chord mode move to the sustain
pedal and a menu page.

When stage 2 lands, the K5–K8 assignments may move to Ond CCs; Morten picks the final layout by
playing it.
