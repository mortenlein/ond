/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Morten Lein */
/* Ond engine: Clipper's DSP (the FM-1 sources, CLIPPER_SYNTH 1) behind a small C API, for the builds that are not
 * the FM-1: the desktop plugin, later the web app. One instance per process: the firmware keeps its state in
 * globals, as on the FM-1. The audio calls (load, MIDI, set, render) from the audio thread only; the table calls
 * (names, info, text, preset values) are pure and safe from any thread.
 * The engine runs at OND_FS; a host at another rate resamples (the plugin does). */
#ifndef OND_ENGINE_H
#define OND_ENGINE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define OND_FS 44100                       /* the FM-1's rate: the DSP's tables are made for it */
#define OND_BLOCK 32                       /* frames per control block (core.h CTL) */

void ond_init(void);                       /* power on: defaults, first engine's first preset */
/* a factory preset, as the FM-1 loads it (ui.c apply_preset_to: sends, arp, voice mode; delay and reverb dry) */
void ond_load_preset(int engine, int preset);
/* the layer's sound (Clipper 0.3's LAYER: track 2 under or beside track 1, voice.c layer_*): its own settings (MODE,
 * SPLIT, OCT, LEVEL, PAN, FILT: the "layer_*" controls) stay as they are */
void ond_load_layer(int engine, int preset);
int ond_engine_count(void);
const char *ond_engine_name(int engine);
int ond_preset_count(int engine);
const char *ond_preset_name(int engine, int preset);

/* one MIDI channel message (note on / off, CC, bend, pressure), as it comes over USB to the FM-1 */
void ond_midi(uint8_t status, uint8_t d1, uint8_t d2);
void ond_all_notes_off(void);
void ond_set_tempo(double bpm);            /* the host's tempo: the arp, a synced delay */
/* the host's transport, before each render: playing, and where the next sample rendered sits in beats from the
 * host's start (a quarter note a beat). The firmware's own transport follows (seq.c: PLAY and STOP as the FM-1's,
 * the position into clk_beat / clk_pos): the arp, the sequencer, the MOD SEQ, the breakers' loops and DUST's
 * record-noise run on the host's grid. Stopped: Ond keeps its own time, as before */
void ond_transport(int playing, double beats);
void ond_play(int on);                     /* the host stopped: Ond's own transport (BEAT's PLAY), from the top */
/* BEAT's drum lanes (16: kick, kick 2, snare, clap, hat, open hat, pedal, rim, snare 2, low tom, high tom, crash, ride,
 * shaker, conga, cowbell: drums.c LANE_NAME), played in (a clip's notes) */
void ond_drum(int lane, int velocity);
/* the hits BEAT's sequencer played in the last render (not the ones played in): lanes and velocities, for MIDI out */
int ond_beat_events(uint8_t *lanes, uint8_t *vels, int max);
/* a kick heard from elsewhere (another Ond's BEAT, LINK): the PUMP ducks as for a kick of its own */
void ond_pump_kick(void);

/* the controls the plugin shows: the firmware's own parameters, by stable keys (append-only: saved sets keep
 * them). EDIT 1..8 are the engine's own (name, range, value text as the FM-1 shows them for that engine). */
enum { OND_SCOPE_GLOBAL, OND_SCOPE_SOUND, OND_SCOPE_EDIT, OND_SCOPE_LAYER, OND_SCOPE_HOST };   /* (HOST: Ond's
                                           * own, not the firmware's: Brudd's CATCH and LAND) */
typedef struct {
    const char *key;                       /* "brite", "edit1", "reverb_type" ... */
    const char *label;                     /* the FM-1's ("BRITE", the engine's "DRAW" ...) */
    const char *section;                   /* "MACROS", "ENGINE", "ENVELOPE", "CHARACTER", "PLAY", "SPACE", "LAYER",
                                            * "BRUDD" */
    int min, max, def;
    int is_enum;                           /* the values are names (ond_control_text) */
    int scope;                             /* OND_SCOPE_* */
    int kept;                              /* kept across a sound change (the player's: sends, level, globals) */
} ond_control_t;
int ond_control_count(void);
void ond_control_info(int c, int engine, ond_control_t *out);
void ond_control_set(int c, int value);   /* clamped to the range of the engine playing */
int ond_control_get(int c);
void ond_control_text(int c, int engine, int value, char *buf, int size);   /* "HALL", "40%", "1.2s" */
int ond_preset_value(int c, int engine, int preset);   /* what the control is once that preset loads */

/* Brudd's breakers (docs/rhythm-breaks.md): SLOOP's punch-in effects on the whole mix, held: 0..count-1 or -1
 * (none). Let go, the mix comes back where it is now (the instrument never stopped underneath) */
int ond_breaker_count(void);
const char *ond_breaker_name(int i);     /* "STUTTER", "STOP", "HALF" ... (pure) */
void ond_breaker(int i);
int ond_breaker_playing(void);           /* the one sounding (fading out included), -1 none */
void ond_breaker_catch(int ms);          /* CATCH: the release fights back over ms (0: SLOOP's 64 samples) */
double ond_tempo(void);                  /* the tempo the engine runs at (the host's, else 90) */

/* Brudd's hits (docs/rhythm-breaks.md, bank A): one-shot recordings the player chose, into the mix before DUST and
 * the breakers. The host owns the samples (OND_FS, -1..1; r may be l for mono) and keeps them until it sets another
 * or NULL; set them from the audio thread (between renders). ond_hit: velocity 1..127 the level (its square, as a
 * key's), a slot playing again fades its last out over 64 samples */
#define OND_NHITS 8
void ond_hit_set(int slot, const float *l, const float *r, uint32_t frames);
/* YOUR KIT: the player's recording for one of BEAT's 16 lanes (as ond_hit_set: the host's samples, OND_FS); a lane
 * with one plays it (at BEAT's level), the others the chosen kit's sound; NULL gives the lane back to the kit */
void ond_kit_set(int lane, const float *l, const float *r, uint32_t frames);
/* bar `bar` of BEAT as it plays with these values (v: FEEL GROOVE DRUNK BARS FILL, then the 7 parts in = 1: KICK SNARE
 * RIM HATS CYMBALS TOMS PERC): per 16th and lane the velocity (0: none), per 16th the nudge in 1/64 of a step. 0: BEAT
 * off. Pure (the tables only): for SAVE MIDI, from the screen's own values */
int ond_beat_bar(const int *v, int bar, uint8_t vel[16][16], int8_t micro[16]);   /* v: 13 values (0.23: v[12] BUSY) */
void ond_hit(int slot, int velocity);

/* n frames (a multiple of OND_BLOCK) of stereo at OND_FS, -1..1 */
void ond_render(float *left, float *right, uint32_t n);

#ifdef __cplusplus
}
#endif
#endif
