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

static const uint8_t OND_G_ID[OND_G_COUNT] = {G_DUST, G_RTYPE, G_RSIZE, G_RDAMP, G_DTIME, G_DFDBK};
static const uint8_t OND_P_ID[OND_P_COUNT] = {P_LEVEL, P_CHOR, P_DLY, P_REV};

/* a factory preset into track 1 as ui.c apply_preset_to sets it (the sound to its defaults first; sends, arp,
 * voice mode; delay and reverb dry, Clipper 1.0) */
static void preset_into(track_t *t, uint32_t e, uint32_t pi)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const engine_t *en = ENGINES[e % NENGINES];
    const preset_t *p;
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = en->edit[i].def;
    if (!en->npresets)
        return;
    pi %= en->npresets;
    p = &en->presets[pi];
    t->preset = (uint8_t)pi;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = p->e[i];
    t->p[P_ATK] = p->env[0];
    t->p[P_DEC] = p->env[1];
    t->p[P_SUS] = p->env[2];
    t->p[P_REL] = p->env[3];
    t->p[P_ED_FLT] = p->fenv;
    t->p[P_ED_FX] = preset_trim(e % NENGINES, pi);
    t->p[P_VOICE] = p->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++) {
        t->p[P_DIST + i] = (int16_t)(p->fx[i] ? p->fx[i] - 1 : FX_DEF[i]);
        if (P_DIST + i == P_DLY || P_DIST + i == P_REV)
            t->p[P_DIST + i] = 0;
        t->p[P_AMODE + i] = (int16_t)(p->arp[i] ? p->arp[i] - 1 : TP[P_AMODE + i].def);
    }
    preset_extras(t->p, p);
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

void ond_set_global(int g, int v)
{
    if (g < 0 || g >= OND_G_COUNT)
        return;
    song.g[OND_G_ID[g]] = (int16_t)clamp(v, GP[OND_G_ID[g]].min, GP[OND_G_ID[g]].max);
}
int ond_get_global(int g) { return g >= 0 && g < OND_G_COUNT ? song.g[OND_G_ID[g]] : 0; }

void ond_set_param(int p, int v)
{
    if (p < 0 || p >= OND_P_COUNT)
        return;
    trk[0].p[OND_P_ID[p]] = (int16_t)clamp(v, TP[OND_P_ID[p]].min, TP[OND_P_ID[p]].max);
}
int ond_get_param(int p) { return p >= 0 && p < OND_P_COUNT ? trk[0].p[OND_P_ID[p]] : 0; }

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
