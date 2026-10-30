/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Morten Lein */
/* Ond engine: Clipper's DSP (the FM-1 sources, CLIPPER_SYNTH 1) behind a small C API, for the builds that are not
 * the FM-1: the desktop plugin, later the web app. One instance per process: the firmware keeps its state in
 * globals, as on the FM-1. Not thread-safe: call everything from the audio thread (or with it stopped).
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
int ond_engine_count(void);
const char *ond_engine_name(int engine);
int ond_preset_count(int engine);
const char *ond_preset_name(int engine, int preset);

/* one MIDI channel message (note on / off, CC, bend, pressure), as it comes over USB to the FM-1 */
void ond_midi(uint8_t status, uint8_t d1, uint8_t d2);
void ond_all_notes_off(void);

/* globals (the GLO and FX pages: DUST, reverb TYPE / SIZE...) and the sound's parameters, by the firmware's ids */
enum { OND_G_DUST, OND_G_RTYPE, OND_G_RSIZE, OND_G_RDAMP, OND_G_DTIME, OND_G_DFDBK, OND_G_COUNT };
enum { OND_P_LEVEL, OND_P_CHOR, OND_P_DLY, OND_P_REV, OND_P_COUNT };
void ond_set_global(int g, int value);    /* clamped to the firmware's range */
int ond_get_global(int g);
void ond_set_param(int p, int value);
int ond_get_param(int p);

/* n frames (a multiple of OND_BLOCK) of stereo at OND_FS, -1..1 */
void ond_render(float *left, float *right, uint32_t n);

#ifdef __cplusplus
}
#endif
#endif
