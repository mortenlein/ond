/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Morten Lein */
/* Ond engine (ond_engine.h): the FM-1's DSP sources in one unit, as tests/hostsim.c builds them, with the FM-1's
 * hardware stubbed: no second core (cpu1_on 0: the voices render here, the firmware's own one-core path), no
 * keys, no IRQs. The audio path is the firmware's mix_block, unchanged. */
#include <stdint.h>
#include <string.h>
#ifndef CLIPPER_SYNTH
#define CLIPPER_SYNTH 1
#endif
#define OND_PUNCH 1                     /* the punch-in effects as Brudd's breakers (punch.c: the ring's 64 KB) */
#ifndef __GNUC__
#define __attribute__(x)                /* (MSVC: the firmware's section / alignment attributes) */
#endif
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include "ond_engine.h"

static struct { volatile uint32_t notes, buttons; } fm1_in;   /* the FM-1's keys: none here */
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
#include "../firmware/src/core.h"
#include "../firmware/src/cpu1.h"
static int cpu1_on(void) { return 0; }   /* no second core: the callers render the voices themselves */
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn; (void)arg; return 0; }
static int cpu1_wait(void) { return 0; }
static uint32_t cpu1_ticks(void) { return 0; }
#include "../firmware/src/engines.c"
#include "../firmware/src/drums.c"
#include "../firmware/src/params.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/slicer.c"
#include "../firmware/src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"
#if FELUCCA_ARRANGER
#include "../firmware/src/arranger.c"
#endif
#include "../firmware/src/seq.c"

/* a factory preset's sound parameters as ui.c apply_preset_to sets them (the sound to its defaults first; sends,
 * arp, voice mode; delay and reverb dry, Clipper 1.0). Pure: no voice, no state touched */
static void preset_params(int16_t *p, uint32_t e, uint32_t pi)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const engine_t *en = ENGINES[e % NENGINES];
    const preset_t *pr;
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        p[P_E0 + i] = en->edit[i].def;
    if (!en->npresets)
        return;
    pr = &en->presets[pi % en->npresets];
    for (i = 0; i < 8u; i++)
        p[P_E0 + i] = pr->e[i];
    p[P_ATK] = pr->env[0];
    p[P_DEC] = pr->env[1];
    p[P_SUS] = pr->env[2];
    p[P_REL] = pr->env[3];
    p[P_ED_FLT] = pr->fenv;
    p[P_ED_FX] = preset_trim(e % NENGINES, pi % en->npresets);
    p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++) {
        p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
        if (P_DIST + i == P_DLY || P_DIST + i == P_REV)
            p[P_DIST + i] = 0;
        p[P_AMODE + i] = (int16_t)(pr->arp[i] ? pr->arp[i] - 1 : TP[P_AMODE + i].def);
    }
    preset_extras(p, pr);
}

static void preset_into(track_t *t, uint32_t e, uint32_t pi)
{
    preset_params(t->p, e, pi);
    if (ENGINES[e % NENGINES]->npresets)
        t->preset = (uint8_t)(pi % ENGINES[e % NENGINES]->npresets);
    mseq_init(t);
    fm6_track_loaded(t);
}

void ond_init(void)
{
    uint32_t i, k;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
    song.master_q12 = 4096;
    preset_into(&trk[0], 0, 0);
    trk[0].eng_req = trk[0].engine = 0;
}

void ond_load_preset(int engine, int preset)
{
    track_t *t = &trk[0];
    uint32_t e = (uint32_t)(engine < 0 ? 0 : engine) % NENGINES;
    sound_fade_begin(t);                        /* as the UI: the old sound fades, the audio side switches */
    preset_into(t, e, (uint32_t)(preset < 0 ? 0 : preset));
    t->eng_req = (uint8_t)e;
}

int ond_engine_count(void) { return (int)NENGINES; }
const char *ond_engine_name(int e) { return e >= 0 && e < (int)NENGINES ? ENGINES[e]->name : ""; }
int ond_preset_count(int e) { return e >= 0 && e < (int)NENGINES ? (int)ENGINES[e]->npresets : 0; }
const char *ond_preset_name(int e, int p)
{
    return p >= 0 && p < ond_preset_count(e) ? ENGINES[e]->presets[p].name : "";
}

void ond_midi(uint8_t status, uint8_t d1, uint8_t d2)
{
    uint32_t cin = status >> 4;                 /* a USB-MIDI event packet, cable 0 (usb.c midi_in_event) */
    if (cin < 8u || cin > 0xEu)
        return;
    midi_in_event(cin | (uint32_t)status << 8 | (uint32_t)(d1 & 0x7Fu) << 16 | (uint32_t)(d2 & 0x7Fu) << 24);
}

void ond_all_notes_off(void)
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        trk_note_off(&trk[0], n);
}

void ond_set_tempo(double bpm)
{
    if (bpm > 0.0)
        song.g[G_BPM] = (int16_t)clamp((int32_t)(bpm + 0.5), GP[G_BPM].min, GP[G_BPM].max);
}

/* ---- the controls (ond_engine.h). Append-only: a control's index is its plugin parameter, saved in Live sets */
typedef struct {
    const char *key, *section;
    uint8_t scope;
    uint16_t id;                               /* G_* (global), P_* (sound), 0..7 (EDIT) */
    uint8_t kept;
} ctl_t;
static const ctl_t CTL_TAB[] = {
    {"brite", "MACROS", OND_SCOPE_SOUND, P_MAC1, 0},
    {"timbr", "MACROS", OND_SCOPE_SOUND, P_MAC2, 0},
    {"motn", "MACROS", OND_SCOPE_SOUND, P_MAC3, 0},
    {"space", "MACROS", OND_SCOPE_SOUND, P_MAC4, 0},
    {"edit1", "ENGINE", OND_SCOPE_EDIT, 0, 0},
    {"edit2", "ENGINE", OND_SCOPE_EDIT, 1, 0},
    {"edit3", "ENGINE", OND_SCOPE_EDIT, 2, 0},
    {"edit4", "ENGINE", OND_SCOPE_EDIT, 3, 0},
    {"edit5", "ENGINE", OND_SCOPE_EDIT, 4, 0},
    {"edit6", "ENGINE", OND_SCOPE_EDIT, 5, 0},
    {"edit7", "ENGINE", OND_SCOPE_EDIT, 6, 0},
    {"edit8", "ENGINE", OND_SCOPE_EDIT, 7, 0},
    {"attack", "ENVELOPE", OND_SCOPE_SOUND, P_ATK, 0},
    {"decay", "ENVELOPE", OND_SCOPE_SOUND, P_DEC, 0},
    {"sustain", "ENVELOPE", OND_SCOPE_SOUND, P_SUS, 0},
    {"release", "ENVELOPE", OND_SCOPE_SOUND, P_REL, 0},
    {"vintage", "CHARACTER", OND_SCOPE_SOUND, P_VINT, 0},
    {"spread", "CHARACTER", OND_SCOPE_SOUND, P_SPREAD, 0},
    {"glide", "CHARACTER", OND_SCOPE_SOUND, P_GLIDE, 0},
    {"detune", "CHARACTER", OND_SCOPE_SOUND, P_DETUNE, 0},
    {"voice", "CHARACTER", OND_SCOPE_SOUND, P_VOICE, 0},
    {"lfo_rate", "CHARACTER", OND_SCOPE_SOUND, P_LRATE, 0},
    {"lfo_wave", "CHARACTER", OND_SCOPE_SOUND, P_LWAVE, 0},
    {"lfo_pitch", "CHARACTER", OND_SCOPE_SOUND, P_LD_PIT, 0},
    {"lfo_filter", "CHARACTER", OND_SCOPE_SOUND, P_LD_FLT, 0},
    {"arp_mode", "PLAY", OND_SCOPE_SOUND, P_AMODE, 0},
    {"arp_rate", "PLAY", OND_SCOPE_SOUND, P_ARATE, 0},
    {"arp_oct", "PLAY", OND_SCOPE_SOUND, P_AOCT, 0},
    {"arp_gate", "PLAY", OND_SCOPE_SOUND, P_AGATE, 0},
    {"arp_hold", "PLAY", OND_SCOPE_SOUND, P_AHOLD, 0},   /* (CHORD, STRUM: the FM-1's own keys only, not MIDI) */
    {"arp_prob", "PLAY", OND_SCOPE_SOUND, P_APROB, 0},
    {"spice", "PLAY", OND_SCOPE_SOUND, P_SPICE, 0},
    {"dust", "SPACE", OND_SCOPE_GLOBAL, G_DUST, 1},
    {"filter", "SPACE", OND_SCOPE_GLOBAL, G_FILT, 1},
    {"chorus", "SPACE", OND_SCOPE_SOUND, P_CHOR, 0},
    {"chorus_rate", "SPACE", OND_SCOPE_GLOBAL, G_CRATE, 1},
    {"chorus_depth", "SPACE", OND_SCOPE_GLOBAL, G_CDEPTH, 1},
    {"delay", "SPACE", OND_SCOPE_SOUND, P_DLY, 1},
    {"delay_time", "SPACE", OND_SCOPE_GLOBAL, G_DTIME, 1},
    {"delay_feedback", "SPACE", OND_SCOPE_GLOBAL, G_DFDBK, 1},
    {"delay_mode", "SPACE", OND_SCOPE_GLOBAL, G_DMODE, 1},
    {"reverb", "SPACE", OND_SCOPE_SOUND, P_REV, 1},
    {"reverb_type", "SPACE", OND_SCOPE_GLOBAL, G_RTYPE, 1},
    {"reverb_size", "SPACE", OND_SCOPE_GLOBAL, G_RSIZE, 1},
    {"reverb_damp", "SPACE", OND_SCOPE_GLOBAL, G_RDAMP, 1},
    {"level", "SPACE", OND_SCOPE_SOUND, P_LEVEL, 1},
};

static const param_desc_t *ctl_desc(int c, int engine)
{
    const ctl_t *k = &CTL_TAB[c];
    if (k->scope == OND_SCOPE_EDIT)
        return &ENGINES[(uint32_t)(engine < 0 ? 0 : engine) % NENGINES]->edit[k->id];
    return k->scope == OND_SCOPE_GLOBAL ? &GP[k->id] : &TP[k->id];
}

int ond_control_count(void) { return (int)NELEM(CTL_TAB); }

void ond_control_info(int c, int engine, ond_control_t *o)
{
    const param_desc_t *d;
    if (c < 0 || c >= ond_control_count())
        c = 0;
    d = ctl_desc(c, engine);
    o->key = CTL_TAB[c].key;
    o->label = d->label && d->label[0] ? d->label : "-";
    o->section = CTL_TAB[c].section;
    o->min = d->min, o->max = d->max, o->def = d->def;
    o->is_enum = d->fmt == F_ENUM;
    o->scope = CTL_TAB[c].scope;
    o->kept = CTL_TAB[c].kept;
}

void ond_control_set(int c, int v)
{
    const ctl_t *k;
    const param_desc_t *d;
    if (c < 0 || c >= ond_control_count())
        return;
    k = &CTL_TAB[c];
    d = ctl_desc(c, trk[0].eng_req);
    v = clamp(v, d->min, d->max);
    if (k->scope == OND_SCOPE_GLOBAL)
        song.g[k->id] = (int16_t)v;
    else
        trk[0].p[k->scope == OND_SCOPE_EDIT ? P_E0 + k->id : k->id] = (int16_t)v;
}

int ond_control_get(int c)
{
    const ctl_t *k;
    if (c < 0 || c >= ond_control_count())
        return 0;
    k = &CTL_TAB[c];
    if (k->scope == OND_SCOPE_GLOBAL)
        return song.g[k->id];
    return trk[0].p[k->scope == OND_SCOPE_EDIT ? P_E0 + k->id : k->id];
}

void ond_control_text(int c, int engine, int v, char *buf, int size)
{
    char val[24];
    const char *unit = "";
    int n = 0, i;
    if (c < 0 || c >= ond_control_count() || size < 2) {
        if (size > 0)
            buf[0] = 0;
        return;
    }
    val[0] = 0;
    param_format(ctl_desc(c, engine), v, val, &unit);
    for (i = 0; val[i] && n < size - 1; i++)
        buf[n++] = val[i];
    for (i = 0; unit && unit[i] && n < size - 1; i++)
        buf[n++] = unit[i];
    buf[n] = 0;
}

int ond_preset_value(int c, int engine, int preset)
{
    int16_t p[P_COUNT];
    const ctl_t *k;
    if (c < 0 || c >= ond_control_count())
        return 0;
    k = &CTL_TAB[c];
    if (k->scope == OND_SCOPE_GLOBAL || k->kept)
        return ctl_desc(c, engine)->def;
    preset_params(p, (uint32_t)(engine < 0 ? 0 : engine), (uint32_t)(preset < 0 ? 0 : preset));
    return p[k->scope == OND_SCOPE_EDIT ? P_E0 + k->id : k->id];
}

/* ---- Brudd's breakers: SLOOP's punch-in effects (punch.c) on the whole mix, one at a time while held */
int ond_breaker_count(void) { return (int)PUNCH_NFX; }
const char *ond_breaker_name(int i) { return i >= 0 && i < (int)PUNCH_NFX ? PUNCH_NAME[i] : ""; }
void ond_breaker(int i) { punch.req = (int8_t)(i >= 0 && i < (int)PUNCH_NFX ? i : -1); }
int ond_breaker_playing(void) { return punch.cur; }

void ond_render(float *l, float *r, uint32_t n)
{
    int32_t o[2 * CTL];
    uint32_t f, i;
    for (f = 0; f + CTL <= n; f += CTL) {
        wt_service();                           /* the main loop's WAVE service before each block (main.c) */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t a = o[2 * i], b = o[2 * i + 1];
            l[f + i] = (float)(a > 32767 ? 32767 : a < -32768 ? -32768 : a) * (1.0f / 32768.0f);
            r[f + i] = (float)(b > 32767 ? 32767 : b < -32768 ? -32768 : b) * (1.0f / 32768.0f);
        }
    }
}
