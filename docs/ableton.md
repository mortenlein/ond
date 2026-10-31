# Ond in Ableton Live

## Ond's low notes: Brudd (0.8) and BEAT's drums (0.10)

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
| E-1 … G-0 | 16–31 | BEAT's drums: 16 kick, 17 kick 2, 18 snare, 19 clap, 20 hat, 21 open hat, 22 pedal hat, 23 rim, 24 snare 2, 25 low tom, 26 high tom, 27 crash, 28 ride, 29 shaker, 30 conga, 31 cowbell (the kit chosen in BEAT) |

A hit plays on the note-on, its velocity the level. A breaker holds while the note does (in a clip: the note's
length), then lets go with CATCH and LAND as set in the BRUDD view.

## The MPK Mini's pads

In Akai's editor for the MPK Mini (the program the pads use), set the pads' notes, then send the program to the MPK
and select it:

- Bank A, pads 1–8: notes 0–7 (C-2 … G-2): the hits.
- Bank B, pads 1–8: notes 8–15 (G#-2 … D#-1): the breakers.

The channel does not matter (Live hands Ond everything on channel 1). Keep the pads on notes, not CCs: notes record
into a clip as rhythm you can edit.

## A beat into a clip (Ond 0.10)

BEAT sends what it plays out of Ond as MIDI notes 16–31, and notes 16–31 coming in play the same drums. So a beat
Ond made up becomes an ordinary Live clip:

1. In Ond's BEAT view: NEW until you like it.
2. A second MIDI track: **MIDI From: (Ond's track) → Ond**, Monitor **Off**, armed.
3. Play Live and record 2, 4 or 8 bars on that track. The beat is now a clip: notes 16–31 in the piano roll.
4. In Ond: FEEL **OFF**. Move (or copy) the clip onto Ond's own track: it plays Ond's drums, as recorded — edit
   it as you like.

(Checked outside Live: the notes Ond sends, played back in, give the same beat within a millisecond.)

**First time, check in Live** (Ond cannot see this from inside): does the second track's "MIDI From" list show
Ond, and does recording put notes 16–31 in the clip? If not, tell Claude what the list shows.

BEAT's PLAY (in its view) runs the beat with Live stopped, on Ond's own time at Live's tempo; Live's play always
takes over. The status line says which.

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
