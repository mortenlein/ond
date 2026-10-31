# Task 014 — Ond in Ableton with recorded MIDI

**Goal:** a performance on Ond — the keys (Roland), the pads (MPK: breakers, hits), the knobs — recorded into a Live
MIDI clip plays back the same, in time with Live's bars, every time; it can be edited in the piano roll, looped,
overdubbed, frozen and bounced.

## How a recorded clip reaches Ond (today)

| What you do | What Live records | On playback |
| --- | --- | --- |
| Keys (Roland, MPK keys) | notes in the clip | notes to Ond (Live sends all on channel 1) |
| MPK pads as **notes**, learned (LEARN) | notes in the clip (36–51 by default) | the same notes: the learned pads hold the breakers / play the hits |
| MPK pads as **CCs**, learned | CC envelopes in the clip | CCs to Ond (through DPF's MIDI mapping in VST3): to be verified in Live |
| Ond's screen: knobs, breaker / hit pads | **automation** (Automation Arm on), per parameter: every knob, `Breaker`, `Hit` | the parameters move; Ond follows |
| A sound picked in Ond's browser | automation of `Sound` **and** of every value the sound sets (the screen sends them) | the same sound, the same values |
| Mod wheel, pitch bend, sustain | in the clip | to Ond |

## What is missing, in order

### 1. Ond follows Live's transport (the base for everything else)

Today Ond keeps its own time: the arp runs from the first key, a breaker's loop starts where it is pressed, the
MOD SEQ and the DUST record-noise never see PLAY. A recorded clip then plays back with the arp and the loops at
another phase than when it was played.

- The plugin hands Live's state to the firmware's own transport clock each block (`ond_transport`): PLAY / STOP
  (`seq_reset_tracks` / `seq_stop`, as the FM-1's PLAY), and Live's beat position into `clk_beat` / `clk_pos`,
  corrected for the samples Ond renders ahead, so nothing drifts (Live's tempo is fractional, Clipper's BPM whole).
  A jump (a loop, a clip launched, a click in the timeline) moves Ond there.
- Then, by Clipper's own code: the arp on Live's 16ths, the breaker loops (LOOP 4 … STUTTER) from the last
  division of Live's grid, LAND on it, the MOD SEQ's SYNC on Live's bars, the delay's tempo.
- **A sound change (for Morten's ear):** DUST's hiss and crackle are "the record turning": on the FM-1 they run
  only while PLAY. Following Live, they run while Live plays and stop when it stops. Ond in the plugin has never
  had them until now.
- Test: `clap_render` with a transport (bars, tempo 92.5, a loop jump): the same clip rendered twice is the same;
  the arp's notes land on the 16ths of the host's grid.

### 2. Pads that record cleanly next to the Roland

Live merges all controllers onto channel 1, so a pad note and the same key on the Roland are the same note.
Two ways, Morten picks:

- **A. Pads on notes no keyboard plays** (MPK editor): e.g. 0–15 (below an 88-key's A0 = 21). Recorded as notes,
  visible and editable in the piano roll as their own rows, LEARN once. Recommended: a clip is plain notes.
- **B. Pads as CCs**: no collision, but recorded as CC envelopes (harder to edit as rhythm), and Live's CC path to
  VST3 plugins must be verified first.

Either way: a fixed **default map** (no LEARN needed) so a clip means the same in every set: if A, Ond answers
notes 0–7 = hits 1–8 and 8–15 = breakers (Morten's eight), on any channel, out of the box; LEARN stays to override.

### 3. Sound changes inside a clip

Live sends no program changes to VST3 plugins. A clip picks its sound by **clip automation of `Sound`** (an
envelope in the clip): Ond loads the whole sound from that alone (it already does: a sound set by the host alone
loads whole; only what the host or the screen set goes over it). To check: a clip's `Sound` envelope with no
other envelopes plays the factory sound exactly.

### 4. Loops and overdub: Live's clip is the pad loop

In Live, Brudd's pad loop (`docs/rhythm-breaks.md`, stage 3) is a 4- or 8-bar MIDI clip with **overdub**
(Session record with MIDI Arrangement Overdub on / clip overdub): every pass adds the pads played, as the doc
asks. Ond's own looper (stage 3) is then for the standalone, the FM-1 and the phone. Ond's part in Live:
"wear over time" (each pass a touch later, a touch dustier), counted from Live's bar position — later, by ear.

### 5. More than one Ond in a set

The firmware keeps its state in globals: one engine per process, a second Ond stays silent. For a set with an
organ track and a drone track: the engine (`host/ond_engine.c`, already a C API) becomes its own DLL that each Ond
loads from its own copy (`%TEMP%\ond-engine-<n>.dll`): its own globals, the same code. No DSP change.

### 6. Freeze, bounce, export

- Offline rendering (Live's Freeze / Export): Ond runs the same code faster than real time; check VST3's offline
  mode reaches DPF and nothing waits on the clock.
- "Nothing starts identical" (`docs/sound-lessons.md` §1: free-running phases, VINTAGE's per-voice drift) means two
  bounces of one clip differ in their details, as two takes on the instrument do. If Morten wants a bounce to equal
  the take: an option to start every PLAY from the same state (seeded). His call.

### 7. Reading a clip

Live names piano-roll rows only for Drum Racks. Ond's BRUDD view shows each pad's note; a printable map
(`docs/ableton.md`) of which note is which breaker / hit, and the Roland's range.

## Order of work

1 (transport) → 2 (default pad notes, after Morten picks A or B) → 3 (check) → 5 (more than one Ond) → 6 → 4's
wear → 7. Step 1 is a sound change only in DUST's hiss following Live's PLAY: renders for Morten.

## Done when
- Morten records keys + pads + knob moves into a clip, it loops in time with Live's bars, and plays back the same.

## Step 1 (2026-10-31): Ond follows Live's transport — ready to listen, `refs/014-1/`

- `ond_transport` (engine) + the plugin: Live's PLAY / STOP and beat position into the firmware's transport
  (`seq_reset_tracks`, `seq_stop`, `clk_beat` / `clk_pos`), corrected for what Ond renders ahead.
- Measured (the engine driven as the plugin drives it, 92.5 BPM, 512-frame blocks): every arp note within 0.70 ms
  of the host's 16ths (one engine block is 0.73 ms). Through the plugin at 44.1 and 48 kHz and round an 8-beat
  loop: the arp on the host's grid; free-running it is anywhere.
- Live stopped: as before (the baseline's samples).
- For Morten's ear: DUST's hiss and crackle now run while Live plays (`before_dust60_live-stopped.wav`,
  `after_dust60_live-playing.wav`), as on the FM-1 with PLAY. The arp files: `arp_free.wav` (its own time) and
  `arp_live-*.wav` (Live's grid).

## Step 2 (2026-10-31): Morten chose A — Brudd's own notes 0–15

Notes 0–7 = hits 1–8, 8–15 = the eight breakers, any channel, nothing to learn (LEARN overrides). Map and the MPK
editor settings: `docs/ableton.md`. Tested through the plugin: note 9 held = STOP (it lands on its frame: 58
samples, where the `Breaker` parameter, applied per host block, can be 221 early); note 21 (A0) plays as a key.
