/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Morten Lein */
/* A minimal CLAP host for testing the built plugin on Windows, without a DAW: loads Ond.clap, sets SOUND and DUST,
 * holds C4 E4 G4 from 0.1 s to 9 s (as tools/render_refs.sh), 12 s in all, at RATE, and writes a 16-bit WAV.
 * A second instance is made, activated and run alongside (0.18: an engine of its own: it plays too). second=N gives
 * it SOUND N (else the same), second_out=FILE writes its audio (a check that the two do not touch each other).
 * 1:NAME=V / 2:NAME=V (after @T: too): a parameter for that instance only; chord=1: the C-E-G to the first only
 * (LINK's tests: the second hears it through the hub).
 *   clap_render PLUGIN.clap OUT.wav [RATE [SOUND [DUST [NAME=VALUE | @SECONDS:NAME=VALUE ...]]]]
 * (NAME: a parameter's name; @SECONDS: set at that time, e.g. @4.0:Breaker=7 @6.0:Breaker=0)
 * transport=BPM: the host plays from beat 0 at BPM (4/4), as Live's transport; loop=A:B: it loops beats A..B
 * @T:note=N, @T:off=N: a key down / up at T (e.g. Brudd's own notes 0..15) */
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
#include "fixedpoint.h"

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
static FILE *midi_log;                          /* midiout=FILE: the plugin's MIDI out, "seconds note velocity" */
static uint64_t block_frame;
static double log_rate;
static bool out_push(const clap_output_events_t *l, const clap_event_header_t *e)
{
    (void)l;
    if (midi_log && l->ctx == (void *)1) {      /* (the first instance's only) */
        if (e->type == CLAP_EVENT_MIDI) {
            const clap_event_midi_t *m = (const clap_event_midi_t *)e;
            if ((m->data[0] & 0xF0) == 0x90 && m->data[2])
                fprintf(midi_log, "%.6f %d %d\n", (double)(block_frame + e->time) / log_rate, m->data[1], m->data[2]);
        } else if (e->type == CLAP_EVENT_NOTE_ON) {
            const clap_event_note_t *n = (const clap_event_note_t *)e;
            fprintf(midi_log, "%.6f %d %d\n", (double)(block_frame + e->time) / log_rate, n->key, (int)(n->velocity * 127.0 + 0.5));
        }
    }
    return true;
}

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
    double rate = argc > 3 ? atof(argv[3]) : 48000.0, bpm = 0.0, loop_a = 0.0, loop_b = 0.0, beats = 0.0;
    int second = -1, chord1 = 0;
    double stop_at = -1.0;                       /* stop=SECONDS: Live's transport stops there (tempo stays) */
    FILE *w2 = NULL;
    clap_event_transport_t tr;
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
    for (int a = 6; a < argc; a++) {
        if (!strncmp(argv[a], "transport=", 10))
            bpm = atof(argv[a] + 10);
        else if (!strncmp(argv[a], "loop=", 5))
            sscanf(argv[a] + 5, "%lf:%lf", &loop_a, &loop_b);
        else if (!strncmp(argv[a], "midiout=", 8))
            midi_log = fopen(argv[a] + 8, "w");
        else if (!strncmp(argv[a], "second=", 7))
            second = atoi(argv[a] + 7);
        else if (!strcmp(argv[a], "chord=1"))
            chord1 = 1;
        else if (!strncmp(argv[a], "stop=", 5))
            stop_at = atof(argv[a] + 5);
        else if (!strncmp(argv[a], "second_out=", 11) && (w2 = fopen(argv[a] + 11, "wb")))
            fwrite("RIFF....WAVE", 1, 44, w2);   /* (the header's place: written at the end) */
    }
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
            clap_output_events_t outev = {(void *)(uintptr_t)(c + 1u), out_push};
            block_frame = f, log_rate = rate;
            clap_audio_buffer_t ob = {chan[c], NULL, 2, 0, 0};
            clap_process_t pr = {(int64_t)f, n, NULL, NULL, &ob, 0, 1, &in, &outev};
            if (bpm > 0.0) {                    /* the host's transport at this block's first frame */
                memset(&tr, 0, sizeof tr);
                tr.header.size = sizeof tr, tr.header.type = CLAP_EVENT_TRANSPORT;
                tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE |
                           CLAP_TRANSPORT_HAS_TIME_SIGNATURE |
                           (stop_at < 0.0 || (double)f / rate < stop_at ? CLAP_TRANSPORT_IS_PLAYING : 0);
                tr.song_pos_beats = (clap_beattime)llround(beats * (double)CLAP_BEATTIME_FACTOR);
                tr.tempo = bpm;
                tr.bar_number = (int32_t)(beats / 4.0);
                tr.bar_start = (clap_beattime)llround((double)tr.bar_number * 4.0 * (double)CLAP_BEATTIME_FACTOR);
                tr.tsig_num = 4, tr.tsig_denom = 4;
                pr.transport = &tr;
            }
            if (!f) {
                add_param(&el, param_id(pl[c], "Sound"), c == 1u && second >= 0 ? second : sound);
                add_param(&el, param_id(pl[c], "Dust"), dust);
            }
            for (int a = 6; a < argc && el.n < MAXEV - 6u; a++) {
                char name[64];
                const char *arg = argv[a], *eq;
                uint32_t at = 0;
                if (arg[0] == '@') {            /* @SECONDS:NAME=VALUE */
                    at = (uint32_t)(atof(arg + 1) * rate);
                    if (!(arg = strchr(arg, ':')))
                        continue;
                    arg++;
                }
                if (!strncmp(arg, "transport=", 10) || !strncmp(arg, "loop=", 5) || !strncmp(arg, "midiout=", 8) ||
                    !strncmp(arg, "second", 6) || !strncmp(arg, "chord=", 6) || !strncmp(arg, "stop=", 5))
                    continue;
                if ((arg[0] == '1' || arg[0] == '2') && arg[1] == ':') {   /* one instance's only */
                    if ((uint32_t)(arg[0] - '1') != c)
                        continue;
                    arg += 2;
                }
                if (at < f || at >= f + n || !(eq = strchr(arg, '=')) || eq - arg >= (int)sizeof name)
                    continue;
                memcpy(name, arg, (size_t)(eq - arg));
                name[eq - arg] = 0;
                if (!strcmp(name, "note") || !strcmp(name, "off")) {   /* @T:note=N / @T:off=N: a key */
                    add_note(&el, at - f, !strcmp(name, "note") ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF,
                             (int16_t)atoi(eq + 1));
                    continue;
                }
                add_param(&el, param_id(pl[c], name), atof(eq + 1));
                el.ev[el.n - 1].h.time = at - f;
            }
            if (on >= f && on < f + n && !(chord1 && c))
                add_note(&el, on - f, CLAP_EVENT_NOTE_ON, 60), add_note(&el, on - f, CLAP_EVENT_NOTE_ON, 64),
                    add_note(&el, on - f, CLAP_EVENT_NOTE_ON, 67);
            if (off >= f && off < f + n && !(chord1 && c))
                add_note(&el, off - f, CLAP_EVENT_NOTE_OFF, 60), add_note(&el, off - f, CLAP_EVENT_NOTE_OFF, 64),
                    add_note(&el, off - f, CLAP_EVENT_NOTE_OFF, 67);
            pl[c]->process(pl[c], &pr);
        }
        if (bpm > 0.0) {                        /* the host's clock: on, round the loop */
            beats += (double)n / rate * bpm / 60.0;
            if (loop_b > loop_a && beats >= loop_b)
                beats = loop_a + (beats - loop_b);
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
            if (w2) {
                int16_t s2[2];
                for (c = 0; c < 2u; c++) {
                    float x = buf[1][c][i];
                    s2[c] = (int16_t)lrintf((x > 1.f ? 1.f : x < -1.f ? -1.f : x) * 32767.f);
                }
                fwrite(s2, 2, 2, w2);
            }
        }
    }
    fclose(w);
    if (w2) {                                   /* (its header: the first's, the same shape) */
        FILE *h = fopen(out, "rb");
        unsigned char hdr[44];
        if (h && fread(hdr, 1, 44, h) == 44) {
            fseek(w2, 0, SEEK_SET);
            fwrite(hdr, 1, 44, w2);
        }
        if (h)
            fclose(h);
        fclose(w2);
    }
    for (c = 0; c < 2u; c++) {
        pl[c]->stop_processing(pl[c]);
        pl[c]->deactivate(pl[c]);
        pl[c]->destroy(pl[c]);
    }
    entry->deinit();
    printf("%s at %.0f Hz, sound %d, dust %d: peak %.1f dBFS, rms %.1f dBFS; second instance peak %.1f dBFS\n", out, rate,
           sound, dust, 20 * log10(peak + 1e-12), 10 * log10(sum / frames + 1e-24),
           20 * log10(peak2 + 1e-12));
    return peak > 0.001 ? 0 : 2;
}
