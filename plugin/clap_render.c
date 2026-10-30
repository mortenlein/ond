/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Morten Lein */
/* A minimal CLAP host for testing the built plugin on Windows, without a DAW: loads Ond.clap, sets SOUND and DUST,
 * holds C4 E4 G4 from 0.1 s to 9 s (as tools/render_refs.sh), 12 s in all, at RATE, and writes a 16-bit WAV.
 * A second instance is made, activated and run alongside: it must stay silent (one engine per process).
 *   clap_render PLUGIN.clap OUT.wav [RATE [SOUND [DUST [NAME=VALUE ...]]]]   (NAME: a parameter's name) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "entry.h"
#include "ext/params.h"
#include "plugin-factory.h"
#include "host.h"
#include "process.h"

#define BLOCK 512u
#define MAXEV 64u

static const void *host_ext(const clap_host_t *h, const char *id) { (void)h; (void)id; return NULL; }
static void host_nop(const clap_host_t *h) { (void)h; }
static const clap_host_t HOST = {CLAP_VERSION_INIT, NULL, "ond clap_render", "Ond", "", "0.1",
                                 host_ext, host_nop, host_nop, host_nop};

typedef struct {
    union {
        clap_event_header_t h;
        clap_event_note_t note;
        clap_event_param_value_t param;
    } ev[MAXEV];
    uint32_t n;
} evlist_t;

static uint32_t ev_size(const clap_input_events_t *l) { return ((const evlist_t *)l->ctx)->n; }
static const clap_event_header_t *ev_get(const clap_input_events_t *l, uint32_t i)
{
    return &((const evlist_t *)l->ctx)->ev[i].h;
}
static bool out_push(const clap_output_events_t *l, const clap_event_header_t *e) { (void)l; (void)e; return true; }

static void add_note(evlist_t *l, uint32_t t, uint16_t type, int16_t key)
{
    clap_event_note_t *n = &l->ev[l->n++].note;
    memset(n, 0, sizeof *n);
    n->header.size = sizeof *n, n->header.time = t, n->header.type = type;
    n->note_id = -1, n->channel = 0, n->key = key, n->velocity = 100.0 / 127.0;
}

static void add_param(evlist_t *l, clap_id id, double v)
{
    clap_event_param_value_t *p = &l->ev[l->n++].param;
    memset(p, 0, sizeof *p);
    p->header.size = sizeof *p, p->header.type = CLAP_EVENT_PARAM_VALUE;
    p->param_id = id, p->note_id = -1, p->port_index = -1, p->channel = -1, p->key = -1, p->value = v;
}

static clap_id param_id(const clap_plugin_t *pl, const char *name)
{
    const clap_plugin_params_t *pp = pl->get_extension(pl, CLAP_EXT_PARAMS);
    uint32_t i;
    clap_param_info_t info;
    for (i = 0; pp && i < pp->count(pl); i++)
        if (pp->get_info(pl, i, &info) && !strcmp(info.name, name))
            return info.id;
    fprintf(stderr, "no parameter %s\n", name);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "Ond.clap", *out = argc > 2 ? argv[2] : "ond.wav";
    double rate = argc > 3 ? atof(argv[3]) : 48000.0;
    int sound = argc > 4 ? atoi(argv[4]) : 0, dust = argc > 5 ? atoi(argv[5]) : 0;
    uint32_t frames = (uint32_t)(12.0 * rate), on = (uint32_t)(0.1 * rate), off = (uint32_t)(9.0 * rate), f, i, c;
    HMODULE dll = LoadLibraryA(path);
    const clap_plugin_entry_t *entry = dll ? (const clap_plugin_entry_t *)GetProcAddress(dll, "clap_entry") : NULL;
    const clap_plugin_factory_t *fac;
    const clap_plugin_t *pl[2];
    float buf[2][2][BLOCK], *chan[2][2];
    double peak = 0, sum = 0, peak2 = 0;
    FILE *w;
    uint32_t v32, data = frames * 4u;
    uint16_t v16;
    if (!entry || !entry->init(path)) {
        fprintf(stderr, "cannot load %s\n", path);
        return 1;
    }
    fac = entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    for (c = 0; c < 2u; c++) {
        pl[c] = fac->create_plugin(fac, &HOST, fac->get_plugin_descriptor(fac, 0)->id);
        if (!pl[c] || !pl[c]->init(pl[c]) || !pl[c]->activate(pl[c], rate, 1, BLOCK) || !pl[c]->start_processing(pl[c])) {
            fprintf(stderr, "instance %u did not start\n", c);
            return 1;
        }
        chan[c][0] = buf[c][0], chan[c][1] = buf[c][1];
    }
    if (!(w = fopen(out, "wb")))
        return 1;
    fwrite("RIFF", 1, 4, w); v32 = 36 + data; fwrite(&v32, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w);
    v32 = 16; fwrite(&v32, 4, 1, w); v16 = 1; fwrite(&v16, 2, 1, w); v16 = 2; fwrite(&v16, 2, 1, w);
    v32 = (uint32_t)rate; fwrite(&v32, 4, 1, w); v32 = (uint32_t)rate * 4u; fwrite(&v32, 4, 1, w);
    v16 = 4; fwrite(&v16, 2, 1, w); v16 = 16; fwrite(&v16, 2, 1, w); fwrite("data", 1, 4, w); fwrite(&data, 4, 1, w);
    for (f = 0; f < frames; f += BLOCK) {
        uint32_t n = frames - f < BLOCK ? frames - f : BLOCK;
        for (c = 0; c < 2u; c++) {
            evlist_t el = {0};
            clap_input_events_t in = {&el, ev_size, ev_get};
            clap_output_events_t outev = {NULL, out_push};
            clap_audio_buffer_t ob = {chan[c], NULL, 2, 0, 0};
            clap_process_t pr = {(int64_t)f, n, NULL, NULL, &ob, 0, 1, &in, &outev};
            if (!f) {
                add_param(&el, param_id(pl[c], "Sound"), sound);
                add_param(&el, param_id(pl[c], "Dust"), dust);
                for (int a = 6; a < argc && el.n < MAXEV - 6u; a++) {
                    char name[64];
                    const char *eq = strchr(argv[a], '=');
                    if (!eq || eq - argv[a] >= (int)sizeof name)
                        continue;
                    memcpy(name, argv[a], (size_t)(eq - argv[a]));
                    name[eq - argv[a]] = 0;
                    add_param(&el, param_id(pl[c], name), atof(eq + 1));
                }
            }
            if (on >= f && on < f + n)
                add_note(&el, on - f, CLAP_EVENT_NOTE_ON, 60), add_note(&el, on - f, CLAP_EVENT_NOTE_ON, 64),
                    add_note(&el, on - f, CLAP_EVENT_NOTE_ON, 67);
            if (off >= f && off < f + n)
                add_note(&el, off - f, CLAP_EVENT_NOTE_OFF, 60), add_note(&el, off - f, CLAP_EVENT_NOTE_OFF, 64),
                    add_note(&el, off - f, CLAP_EVENT_NOTE_OFF, 67);
            pl[c]->process(pl[c], &pr);
        }
        for (i = 0; i < n; i++) {
            int16_t s[2];
            for (c = 0; c < 2u; c++) {
                float x = buf[0][c][i];
                s[c] = (int16_t)lrintf((x > 1.f ? 1.f : x < -1.f ? -1.f : x) * 32767.f);
                peak = fabs(x) > peak ? fabs(x) : peak;
                peak2 = fabs(buf[1][c][i]) > peak2 ? fabs(buf[1][c][i]) : peak2;
            }
            sum += (double)buf[0][0][i] * buf[0][0][i];
            fwrite(s, 2, 2, w);
        }
    }
    fclose(w);
    for (c = 0; c < 2u; c++) {
        pl[c]->stop_processing(pl[c]);
        pl[c]->deactivate(pl[c]);
        pl[c]->destroy(pl[c]);
    }
    entry->deinit();
    printf("%s at %.0f Hz, sound %d, dust %d: peak %.1f dBFS, rms %.1f dBFS; second instance peak %s\n", out, rate,
           sound, dust, 20 * log10(peak + 1e-12), 10 * log10(sum / frames + 1e-24),
           peak2 == 0.0 ? "silent (ok)" : "NOT SILENT");
    return peak > 0.001 && peak2 == 0.0 ? 0 : 2;
}
