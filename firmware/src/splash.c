/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP boot splash: the logo (tools/gen_logo.py -> sloop_logo.h: 16 colours, RLE runs of
 * (run - 1) << 4 | colour), drawn into the canvas in bands of at most 120 rows. */
#include "sloop_logo.h"
static void sloop_logo_draw(uint32_t y0)
{
    uint32_t r0;
    for (r0 = 0; r0 < SLOOP_SPLASH_H; r0 += 120u) {
        uint32_t rows = SLOOP_SPLASH_H - r0 > 120u ? 120u : SLOOP_SPLASH_H - r0;
        uint32_t lo = r0 * SLOOP_SPLASH_W, hi = (r0 + rows) * SLOOP_SPLASH_W, pos = 0, i;
        cv_begin(SLOOP_SPLASH_W, rows, C_BLACK);
        for (i = 0; i < sizeof SLOOP_SPLASH_RLE && pos < hi; i++) {
            uint32_t run = (SLOOP_SPLASH_RLE[i] >> 4) + 1u, c = SLOOP_SPLASH_RLE[i] & 15u, p;
            if (c && pos + run > lo)
                for (p = pos < lo ? lo : pos; p < pos + run && p < hi; p++)
                    cv_px[p - lo] = swap16(SLOOP_SPLASH_PAL[c]);
            pos += run;
        }
        cv_blit((240u - SLOOP_SPLASH_W) / 2u, y0 + r0);
    }
}

/* (Ond) the boot screen: the name in the screen font at 4x (FONT_S's bitmaps, as FONT_L is them at 2x: no new
 * glyphs in flash, ~130 B), the version, where it comes from. Clipper's logo and boot screen are kept below as they
 * were (OND_SPLASH=0), so upstream changes to them still merge */
#ifndef OND_SPLASH
#define OND_SPLASH 1
#endif
#if OND_SPLASH
#define XL8(v) v, v, v, v, v, v, v, v
static const uint8_t FONT_XL_ADV[64] = {XL8(32), XL8(32), XL8(32), XL8(32), XL8(32), XL8(32), XL8(32), XL8(32)};
static const uint8_t FONT_XL_BW[64] = {XL8(48), XL8(48), XL8(48), XL8(48), XL8(48), XL8(48), XL8(48), XL8(48)};
static const felucca_font_t FONT_XL = {64, 8, 32, 95, 2, FONT_XL_ADV, FONT_XL_BW, FONT_S_OFF, FONT_S_DATA, 1};
#undef XL8

static void sloop_splash(void)
{
    const char *o = OND_VERSION, *c = FELUCCA_VERSION;
    char vb[24];
    uint32_t n = 0;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, 64, C_BLACK);
    cv_text(120 - text_w(&FONT_XL, "OND") / 2, 0, &FONT_XL, "OND", RGB(232, 228, 216));
    cv_blit(0, 56);
    if (o[0] == 'O' && o[1] == 'N' && o[2] == 'D' && o[3] == ' ')
        o += 4;                                         /* "OND 0.1" -> "0.1" under the name */
    if (c[0] == 'C' && c[1] == 'L' && c[7] == ' ')
        c += 8;                                         /* "CLIPPER 1.3" -> "1.3" */
    str_cpy(vb, "from clipper ", sizeof vb);
    while (vb[n])
        n++;
    str_cpy(vb + n, c, sizeof vb - n);
    cv_begin(240, 16, C_BLACK);
    cv_text(120 - text_w(&FONT_S, o) / 2, 0, &FONT_S, o, RGB(196, 196, 204));
    cv_blit(0, 128);
    cv_begin(240, 36, C_BLACK);
    cv_text(120 - text_w(&FONT_S, vb) / 2, 0, &FONT_S, vb, RGB(96, 96, 104));
    cv_text(120 - text_w(&FONT_S, "based on felucca") / 2, 18, &FONT_S, "based on felucca", RGB(96, 96, 104));
    cv_blit(0, 202);
}
#else
/* the boot screen: the logo, the version, where it comes from */
static void sloop_splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    sloop_logo_draw(10);
    const char *v = FELUCCA_VERSION;
    char vb[24];
    if (v[0] == 'C' && v[1] == 'L' && v[7] == ' ')
        v += 8;                                         /* "CLIPPER 0.1" -> "0.1" under the wordmark */
    {   /* (1.0) "1.0 EXP" -> "1.0 EXPERIMENTAL": the boot screen says it whole */
        uint32_t n = 0;
        while (v[n] && n < 12u)
            vb[n] = v[n], n++;
        vb[n] = 0;
        if (n >= 4u && vb[n - 4] == ' ' && vb[n - 3] == 'E' && vb[n - 2] == 'X' && vb[n - 1] == 'P' && !v[n])
            str_cpy(vb + n, "ERIMENTAL", 10);
        v = vb;
    }
    cv_begin(240, 36, C_BLACK);
    cv_text(120 - text_w(&FONT_S, v) / 2, 0, &FONT_S, v, RGB(196, 196, 204));
    cv_text(120 - text_w(&FONT_S, "based on felucca") / 2, 18, &FONT_S, "based on felucca", RGB(96, 96, 104));
    cv_blit(0, 202);
}
#endif
