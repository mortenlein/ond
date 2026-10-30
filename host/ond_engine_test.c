/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Morten Lein */
/* Ond engine smoke test: a preset, a C-E-G chord over MIDI held 3 s, released, 2 s more; writes a WAV and
 * prints the levels. host/ond_engine_test ENGINE PRESET OUT.wav [DUST] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ond_engine.h"

static int control(const char *key)
{
    ond_control_t c;
    int i;
    for (i = 0; i < ond_control_count(); i++) {
        ond_control_info(i, 0, &c);
        if (!strcmp(c.key, key))
            return i;
    }
    return -1;
}

int main(int argc, char **argv)
{
    int e = argc > 1 ? atoi(argv[1]) : 7, p = argc > 2 ? atoi(argv[2]) : 6;
    const char *out = argc > 3 ? argv[3] : "ond_engine_test.wav";
    uint32_t frames = 5u * OND_FS, f, i, data = frames * 4u;
    static float l[OND_BLOCK], r[OND_BLOCK];
    double peak = 0, sum = 0;
    FILE *w = fopen(out, "wb");
    uint32_t v32;
    uint16_t v16;
    if (!w)
        return 1;
    ond_init();
    ond_load_preset(e, p);
    if (argc > 4)
        ond_control_set(control("dust"), atoi(argv[4]));
    fwrite("RIFF", 1, 4, w); v32 = 36 + data; fwrite(&v32, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w);
    v32 = 16; fwrite(&v32, 4, 1, w); v16 = 1; fwrite(&v16, 2, 1, w); v16 = 2; fwrite(&v16, 2, 1, w);
    v32 = OND_FS; fwrite(&v32, 4, 1, w); v32 = OND_FS * 4; fwrite(&v32, 4, 1, w);
    v16 = 4; fwrite(&v16, 2, 1, w); v16 = 16; fwrite(&v16, 2, 1, w); fwrite("data", 1, 4, w); fwrite(&data, 4, 1, w);
    for (f = 0; f < frames; f += OND_BLOCK) {
        if (f == OND_FS / 10 / OND_BLOCK * OND_BLOCK)
            ond_midi(0x90, 60, 100), ond_midi(0x90, 64, 100), ond_midi(0x90, 67, 100);
        if (f == (OND_FS / 10 + 3u * OND_FS) / OND_BLOCK * OND_BLOCK)
            ond_midi(0x80, 60, 0), ond_midi(0x80, 64, 0), ond_midi(0x80, 67, 0);
        ond_render(l, r, OND_BLOCK);
        for (i = 0; i < OND_BLOCK; i++) {
            int16_t s[2] = {(int16_t)lrintf(l[i] * 32767.0f), (int16_t)lrintf(r[i] * 32767.0f)};
            fwrite(s, 2, 2, w);
            if (fabs(l[i]) > peak)
                peak = fabs(l[i]);
            sum += (double)l[i] * l[i];
        }
    }
    fclose(w);
    printf("%s / %s: peak %.1f dBFS, rms %.1f dBFS\n", ond_engine_name(e), ond_preset_name(e, p),
           20 * log10(peak + 1e-12), 10 * log10(sum / frames + 1e-24));
    return peak > 0.001 ? 0 : 2;
}
