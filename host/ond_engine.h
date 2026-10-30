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

/* n frames (a multiple of OND_BLOCK) of stereo at OND_FS, -1..1 */
void ond_render(float *left, float *right, uint32_t n);

#ifdef __cplusplus
}
#endif
#endif
