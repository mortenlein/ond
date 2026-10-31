# Ond in Ableton Live

## Brudd's notes (Ond 0.8)

Ond answers these notes on any channel, out of the box (no LEARN). They are below every keyboard (an 88-key starts
at A0 = 21), so the Roland never plays them, and in a clip the pads are rows of their own at the bottom of the
piano roll. LEARN in the BRUDD view still takes any note or CC over them.

| Note (Live's name) | Number | Plays |
| --- | --- | --- |
| C-2 | 0 | Hit 1 (the first WAV in `Documents\Ond\Hits`, by name) |
| C#-2 | 1 | Hit 2 |
| D-2 | 2 | Hit 3 |
| D#-2 | 3 | Hit 4 |
| E-2 | 4 | Hit 5 |
| F-2 | 5 | Hit 6 |
| F#-2 | 6 | Hit 7 |
| G-2 | 7 | Hit 8 |
| G#-2 | 8 | STUTTER (held) |
| A-2 | 9 | STOP: tape stop |
| A#-2 | 10 | GATE: dropout |
| B-2 | 11 | CRUSH |
| C-1 | 12 | HALF: half speed |
| C#-1 | 13 | REVERSE |
| D-1 | 14 | LOOP 4: a beat held (freeze) |
| D#-1 | 15 | ECHO |

A hit plays on the note-on, its velocity the level. A breaker holds while the note does (in a clip: the note's
length), then lets go with CATCH and LAND as set in the BRUDD view.

## The MPK Mini's pads

In Akai's editor for the MPK Mini (the program the pads use), set the pads' notes, then send the program to the MPK
and select it:

- Bank A, pads 1–8: notes 0–7 (C-2 … G-2): the hits.
- Bank B, pads 1–8: notes 8–15 (G#-2 … D#-1): the breakers.

The channel does not matter (Live hands Ond everything on channel 1). Keep the pads on notes, not CCs: notes record
into a clip as rhythm you can edit.

## Recording a take

1. Ond's track: input **All Ins** (the Roland and the MPK both), monitor **In** or armed.
2. Record a clip: keys, pads and the mod wheel go into it as notes. To also record knob moves and the screen's
   pads, turn on **Automation Arm**: every knob, `Breaker` and `Hit` are parameters.
3. A clip's sound: a `Sound` envelope in the clip (Live sends no program changes to plugins). The sound loads whole
   from it.
4. Loops and overdub: a 4- or 8-bar clip with overdub is Brudd's pad loop.

While Live plays, Ond runs on Live's grid: the arp, the breakers' loops, LAND, the MOD SEQ (Ond 0.7, measured within
0.7 ms of Live's 16ths). DUST's hiss and crackle run while Live plays, as on the FM-1 with PLAY.

## Limits (docs/tasks/014-ableton-midi.md)

- One Ond per set for now (a second stays silent).
- Bounces of one clip differ in their details (free-running phases, as two takes on the instrument).
