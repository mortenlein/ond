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
#define OND_HITS 1                      /* Brudd's hits into the mix (fx.c mix_block, ond_hits_mix below) */
#define OND_BEAT 1                      /* BEAT: SLOOP's drum machine back on (drums.c, its track's steps; task 015) */
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
/* ---- Brudd's hits (ond_engine.h): per slot the voice playing and the one fading out under a retrigger */
static struct {
    const float *l, *r;
    uint32_t len;
    struct { uint32_t pos; int32_t g, fade; } v[2];   /* g: Q15 gain; fade: samples left of a 64-sample fade, -1 none */
} hits[OND_NHITS];
static void ond_hits_mix(int32_t *ml, int32_t *mr, uint32_t n)
{
    uint32_t s, k, i;
    for (s = 0; s < OND_NHITS; s++) {
        if (!hits[s].l)
            continue;
        for (k = 0; k < 2u; k++) {
            uint32_t pos = hits[s].v[k].pos;
            const int32_t g = hits[s].v[k].g;
            int32_t fade = hits[s].v[k].fade;
            for (i = 0; i < n && pos < hits[s].len && fade != 0; i++, pos++) {
                const float gf = (float)g * (fade > 0 ? (float)fade * (1.0f / 64.0f) : 1.0f) * (32767.0f / 32768.0f);
                ml[i] += (int32_t)(hits[s].l[pos] * gf);
                mr[i] += (int32_t)(hits[s].r[pos] * gf);
                if (fade > 0)
                    fade--;
            }
            hits[s].v[k].pos = fade == 0 ? hits[s].len : pos;
            hits[s].v[k].fade = fade;
        }
    }
}
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

/* the layer's own settings: kept over a sound loaded into it (preset_params sets track 2's whole sound) */
static const uint8_t LAYER_OWN[] = {P_LYMODE, P_LYSPLIT, P_LYOCT, P_LYCH, P_LEVEL, P_PAN, P_TFLT};

static void layer_into(uint32_t e, uint32_t pi)
{
    int16_t keep[NELEM(LAYER_OWN)];
    uint32_t i;
    for (i = 0; i < NELEM(LAYER_OWN); i++)
        keep[i] = trk[1].p[LAYER_OWN[i]];
    preset_into(&trk[1], e, pi);
    for (i = 0; i < NELEM(LAYER_OWN); i++)
        trk[1].p[LAYER_OWN[i]] = keep[i];
}

/* BEAT's pattern into the drum track (task 015 step 1: OFF, or TEST: kick 1 and 3, snare 2 and 4, closed hat on
 * the 8ths, a 16-step bar: the plumbing's check, not a groove; the real ones come from drummers, step 2) */
static void beat_init(void);
static void beat_feel(int feel)
{
    track_t *t = TDRUM;
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        memset(&t->dstep[i], 0, sizeof t->dstep[i]);
        t->micro[i] = 0;
    }
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;                           /* 1/16 */
    if (feel != 1)
        return;
    for (i = 0; i < 16u; i++) {
        if (i % 8u == 0u)
            dstep_set(&t->dstep[i], 0, LV_NORM, 0);   /* kick */
        if (i % 8u == 4u)
            dstep_set(&t->dstep[i], 2, LV_NORM, 0);   /* snare */
        if (i % 2u == 0u)
            dstep_set(&t->dstep[i], 4, i % 4u ? LV_SOFT : LV_NORM, 0);   /* hat */
    }
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
    trk[1].p[P_LYMODE] = LY_OFF;                /* (as params.c's synth defaults: off, split at C4, octave 0) */
    trk[1].p[P_LYSPLIT] = 64;
    trk[1].p[P_LYOCT] = 2;
    trk[1].p[P_LYCH] = 2;
    layer_into(12, 18);                         /* (the layer's first sound: HYBRID DRONE, Ond's drone) */
    trk[1].eng_req = trk[1].engine = 12;
    beat_init();                                /* BEAT: LO-FI, empty (FEEL OFF) */
}

void ond_load_layer(int engine, int preset)
{
    uint32_t e = (uint32_t)(engine < 0 ? 0 : engine) % NENGINES;
    sound_fade_begin(&trk[1]);
    layer_into(e, (uint32_t)(preset < 0 ? 0 : preset));
    trk[1].eng_req = (uint8_t)e;
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

void ond_transport(int playing, double beats)
{
    uint32_t b, pos;
    if (!playing) {
        if (song.playing)
            seq_stop();                         /* (as the FM-1's STOP: the sequencer's and the arp's notes end) */
        return;
    }
    if (beats < 0.0)
        beats = 0.0;
    b = (uint32_t)beats;
    pos = (uint32_t)((beats - (double)b) * (double)BEAT_U);
    if (pos >= BEAT_U)
        pos = BEAT_U - 1u;
    if (!song.playing)
        seq_reset_tracks(pos);                  /* PLAY: every track from here, as a MIDI START does */
    clk_beat = b;                               /* the host's position every block: no drift (its tempo has a */
    clk_pos = pos;                              /* fraction, Clipper's BPM none), a jump or a loop followed */
}

void ond_set_tempo(double bpm)
{
    if (bpm > 0.0)
        song.g[G_BPM] = (int16_t)clamp((int32_t)(bpm + 0.5), GP[G_BPM].min, GP[G_BPM].max);
}

void ond_breaker_catch(int ms);

/* ---- the controls (ond_engine.h). Append-only: a control's index is its plugin parameter, saved in Live sets */
typedef struct {
    const char *key, *section;
    uint8_t scope;
    uint16_t id;                               /* G_* (global), P_* (sound, layer), 0..7 (EDIT) */
    uint8_t kept;
    const param_desc_t *desc;                  /* (the layer's: its slots are the pattern's, their TP is not theirs) */
} ctl_t;
/* the layer's settings as the FM-1 shows them (ui_synth.c XD[X_LYMODE ..]; MULTI left out: one MIDI channel here) */
static const char *const N_OCTS5[] = {"-2", "-1", "0", "+1", "+2"};
static const param_desc_t LY_MODE_D = {"MODE", F_ENUM, LY_OFF, LY_SPLIT, LY_OFF, N_LYMODE, 0};
static const param_desc_t LY_SPLIT_D = {"SPLIT", F_SPLIT, 16, 127, 64, 0, 0};
static const param_desc_t LY_OCT_D = {"OCT", F_ENUM, 0, 4, 2, N_OCTS5, 0};
static const param_desc_t LY_LEVEL_D = {"LEVEL", F_DB, 0, 127, 104, 0, 0};
static const param_desc_t LY_PAN_D = {"PAN", F_BIPCT, -64, 63, 0, 0, 0};
static const param_desc_t LY_FILT_D = {"FILT", F_FILT, -64, 63, 0, 0, 0};
/* Brudd (Ond's own): CATCH, how long a let-go breaker fights its way back (ms; the SL-1200's 0.7 s at 700), LAND,
 * the grid its return waits for (the plugin holds it till then) */
static const char *const N_LAND[] = {"OFF", "1/16", "1/8", "1/4"};
/* BEAT (task 015): FEEL (OFF, TEST: the plumbing's check, not a groove), KIT (the drum track's: SLOOP's synthesised
 * kits; DRUM_KIT_NAMES), LEVEL (its own: drums.c DR_LVL) */
static const char *const N_FEEL[] = {"OFF", "TEST"};
static const param_desc_t FEEL_D = {"FEEL", F_ENUM, 0, 1, 0, N_FEEL, 0};
static const param_desc_t KIT_D = {"KIT", F_ENUM, DRUM_SAMPLED, DRUM_SAMPLED + DS_NKITS - 1, DRUM_SAMPLED + 8,
                                   DRUM_KIT_NAMES, 0};   /* (the 32 synthesised kits; LO-FI first) */
static const param_desc_t BLVL_D = {"LEVEL", F_PCT, 0, 127, 100, 0, 0};
static const param_desc_t CATCH_D = {"CATCH", F_INT, 0, 1000, 0, 0, "ms"};
static const param_desc_t LAND_D = {"LAND", F_ENUM, 0, 3, 0, N_LAND, 0};
static int16_t host_val[4];                    /* CATCH, LAND, FEEL, (beat LEVEL: drums.c's own) */
static void beat_feel(int feel);
static void beat_init(void)
{
    TDRUM->p[P_E0] = (int16_t)KIT_D.def;
    ond_beat_lvl = 100;
    host_val[2] = 0;
    host_val[3] = 100;
    beat_feel(0);
}
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
    {"layer_mode", "LAYER", OND_SCOPE_LAYER, P_LYMODE, 1, &LY_MODE_D},   /* (Ond 0.5) */
    {"layer_oct", "LAYER", OND_SCOPE_LAYER, P_LYOCT, 1, &LY_OCT_D},
    {"layer_split", "LAYER", OND_SCOPE_LAYER, P_LYSPLIT, 1, &LY_SPLIT_D},
    {"layer_level", "LAYER", OND_SCOPE_LAYER, P_LEVEL, 1, &LY_LEVEL_D},
    {"layer_pan", "LAYER", OND_SCOPE_LAYER, P_PAN, 1, &LY_PAN_D},
    {"layer_filter", "LAYER", OND_SCOPE_LAYER, P_TFLT, 1, &LY_FILT_D},
    {"catch", "BRUDD", OND_SCOPE_HOST, 0, 1, &CATCH_D},                  /* (Ond 0.6) */
    {"land", "BRUDD", OND_SCOPE_HOST, 1, 1, &LAND_D},
    {"beat_feel", "BEAT", OND_SCOPE_HOST, 2, 1, &FEEL_D},                /* (Ond 0.9) */
    {"beat_kit", "BEAT", OND_SCOPE_HOST, 4, 1, &KIT_D},
    {"beat_level", "BEAT", OND_SCOPE_HOST, 3, 1, &BLVL_D},
};

static const param_desc_t *ctl_desc(int c, int engine)
{
    const ctl_t *k = &CTL_TAB[c];
    if (k->desc)
        return k->desc;
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
    else if (k->scope == OND_SCOPE_LAYER)
        trk[1].p[k->id] = (int16_t)v;
    else if (k->scope == OND_SCOPE_HOST) {
        if (k->id == 4) {                       /* KIT: the drum track's own slot */
            TDRUM->p[P_E0] = (int16_t)v;
            return;
        }
        host_val[k->id] = (int16_t)v;
        if (k->id == 0)
            ond_breaker_catch(v);
        if (k->id == 2)
            beat_feel(v);
        if (k->id == 3)
            ond_beat_lvl = (int16_t)v;
    } else
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
    if (k->scope == OND_SCOPE_LAYER)
        return trk[1].p[k->id];
    if (k->scope == OND_SCOPE_HOST)
        return k->id == 4 ? TDRUM->p[P_E0] : host_val[k->id];
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
    if (k->scope == OND_SCOPE_GLOBAL || k->scope == OND_SCOPE_LAYER || k->scope == OND_SCOPE_HOST || k->kept)
        return ctl_desc(c, engine)->def;
    preset_params(p, (uint32_t)(engine < 0 ? 0 : engine), (uint32_t)(preset < 0 ? 0 : preset));
    return p[k->scope == OND_SCOPE_EDIT ? P_E0 + k->id : k->id];
}

/* ---- Brudd's breakers: SLOOP's punch-in effects (punch.c) on the whole mix, one at a time while held */
int ond_breaker_count(void) { return (int)PUNCH_NFX; }
const char *ond_breaker_name(int i) { return i >= 0 && i < (int)PUNCH_NFX ? PUNCH_NAME[i] : ""; }
void ond_breaker(int i) { punch.req = (int8_t)(i >= 0 && i < (int)PUNCH_NFX ? i : -1); }
int ond_breaker_playing(void) { return punch.cur; }
void ond_breaker_catch(int ms) { punch_catch = ms > 0 ? (uint32_t)(ms > 2000 ? 2000 : ms) * (uint32_t)FS / 1000u : 0u; }
double ond_tempo(void) { return (double)song.g[G_BPM]; }

void ond_hit_set(int slot, const float *l, const float *r, uint32_t frames)
{
    if (slot < 0 || slot >= OND_NHITS)
        return;
    hits[slot].l = l;
    hits[slot].r = r ? r : l;
    hits[slot].len = l ? frames : 0u;
    hits[slot].v[0].pos = hits[slot].v[1].pos = hits[slot].len;   /* (silent till hit) */
}

void ond_hit(int slot, int velocity)
{
    int32_t v = velocity < 0 ? 0 : velocity > 127 ? 127 : velocity;
    if (slot < 0 || slot >= OND_NHITS || !hits[slot].l || !v)
        return;
    hits[slot].v[1] = hits[slot].v[0];                  /* the last one fades out under the new one */
    if (hits[slot].v[1].pos < hits[slot].len)
        hits[slot].v[1].fade = 64;
    hits[slot].v[0].pos = 0;
    hits[slot].v[0].g = v * v * 2;                      /* (127: 32258, Q15) */
    hits[slot].v[0].fade = -1;
}

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
