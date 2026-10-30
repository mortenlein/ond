// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// Ond's screen (DPF, NanoVG): the sound browser, the four macros, the engine's own EDIT knobs, the envelope,
// character, play and space sections. Every label and value is the FM-1's (the firmware's names and
// param_format, ond_control_text), drawn in the FM-1's own pixel font (Terminus, build/gen/felucca_font.h).
#include "DistrhoUI.hpp"
#include "OndShared.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

typedef struct {                   // (firmware/src/gfx.c: the generated font's shape)
    uint8_t h, pad, first, last, sh;
    const uint8_t *adv, *bw;
    const uint16_t *off;
    const uint8_t *data;
    uint8_t bits;
} felucca_font_t;
#include "felucca_font.h"

START_NAMESPACE_DISTRHO

namespace {

constexpr float W = 1000.f, H = 700.f;

struct Rgb {
    int r, g, b;
};
constexpr Rgb BG{18, 17, 16}, PANEL{27, 26, 23}, EDGE{40, 38, 33}, TEXT{232, 228, 216}, DIM{138, 132, 120},
    FAINT{64, 61, 55}, ACCENT{232, 162, 58}, KNOB{36, 34, 30};

struct Section {
    const char *title;
    float x, y, w, h;
    int cols, rows;
    std::vector<const char *> keys;
};

// the layout: rows of sections, each a grid of knobs (keys: OndShared / host/ond_engine.c)
const std::vector<Section> &sections()
{
    static const std::vector<Section> s = {
        {"MACROS", 16, 84, 424, 240, 4, 1, {"brite", "timbr", "motn", "space"}},
        {"ENGINE", 452, 84, 320, 240, 4, 2, {"edit1", "edit2", "edit3", "edit4", "edit5", "edit6", "edit7", "edit8"}},
        {"ENVELOPE", 784, 84, 200, 240, 4, 1, {"attack", "decay", "sustain", "release"}},
        {"CHARACTER", 16, 336, 320, 348, 5, 2,
         {"vintage", "spread", "glide", "detune", "voice", "lfo_rate", "lfo_wave", "lfo_pitch", "lfo_filter"}},
        {"PLAY", 348, 336, 240, 348, 3, 3,
         {"arp_hold", "arp_mode", "arp_rate", "arp_oct", "arp_gate", "arp_prob", "spice"}},
        {"SPACE", 600, 336, 384, 348, 5, 3,
         {"dust", "filter", "level", "chorus", "chorus_rate", "chorus_depth", "delay", "delay_time", "delay_feedback",
          "delay_mode", "reverb", "reverb_type", "reverb_size", "reverb_damp"}},
    };
    return s;
}

struct Hit {
    float x, y, w, h;
    int param;                     // >= 0 a knob; -10 previous sound, -11 next, -12 the browser
};

} // namespace

class OndUI : public UI {
public:
    OndUI()
        : UI((uint)W, (uint)H), vals((size_t)ond::paramCount(), 0.f)
    {
        const double sf = getScaleFactor();
        setGeometryConstraints((uint)(W * sf), (uint)(H * sf), true);
        if (sf != 1.0)
            setSize((uint)(W * sf), (uint)(H * sf));
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= vals.size() || vals[index] == value)
            return;
        vals[index] = value;
        repaint();
    }

    void onNanoDisplay() override
    {
        const float sf = (float)getWidth() / W;
        hits.clear();
        save();
        scale(sf, sf);
        fill(0, 0, W, H, BG);
        drawHeader();
        if (browserOpen)
            drawBrowser();
        else
            for (const Section &s : sections())
                drawSection(s);
        restore();
    }

    bool onMouse(const MouseEvent &ev) override
    {
        const float sf = (float)getWidth() / W;
        const float x = (float)ev.pos.getX() / sf, y = (float)ev.pos.getY() / sf;
        if (ev.button != 1)
            return false;
        if (!ev.press) {
            if (drag >= 0) {
                if (!moved && isEnum(drag))
                    stepValue(drag, 1);              // a click on a named value: the next one
                editParameter((uint32_t)drag, false);
                drag = -1;
                repaint();
            }
            return true;
        }
        if (browserOpen)
            return browserClick(x, y);
        for (const Hit &h : hits) {
            if (x < h.x || x >= h.x + h.w || y < h.y || y >= h.y + h.h)
                continue;
            if (h.param == -10 || h.param == -11) {
                const int n = (int)ond::sounds().size();
                selectSound((soundIndex() + (h.param == -11 ? 1 : n - 1)) % n);
            } else if (h.param == -12) {
                browserOpen = true;
                browserEngine = currentEngine();
                browserFav = soundIndex() < ond::FAVOURITES;
            } else if (h.param >= 0) {
                if (ev.time - lastClick < 350 && lastParam == h.param) {
                    resetValue(h.param);             // double click: as the sound has it
                    lastClick = 0;
                    return true;
                }
                lastClick = ev.time, lastParam = h.param;
                drag = h.param, dragY = y, dragStart = vals[(size_t)h.param], moved = false;
                editParameter((uint32_t)drag, true);
            }
            repaint();
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent &ev) override
    {
        if (drag < 0)
            return false;
        const float sf = (float)getWidth() / W;
        const float dy = dragY - (float)ev.pos.getY() / sf;
        if (std::fabs(dy) > 2.f)
            moved = true;
        if (!moved)
            return true;
        ond_control_t info;
        controlInfo(drag, info);
        const float fine = (ev.mod & kModifierShift) ? 0.2f : 1.f;
        if (info.scope == OND_SCOPE_EDIT) {
            const int steps = info.max - info.min;
            const float perPx = steps > 24 ? 1.f / 200.f : 1.f / (12.f * (float)(steps > 0 ? steps : 1));
            setValue(drag, std::fmin(1.f, std::fmax(0.f, dragStart + dy * perPx * fine)), false);
        } else {
            const float range = (float)(info.max - info.min);
            const float perPx = info.is_enum || range <= 24.f ? 1.f / 12.f : range / 200.f;
            setValue(drag, std::round(std::fmin((float)info.max, std::fmax((float)info.min,
                                                                             dragStart + dy * perPx * fine))),
                     false);
        }
        return true;
    }

    bool onScroll(const ScrollEvent &ev) override
    {
        const float sf = (float)getWidth() / W;
        const float x = (float)ev.pos.getX() / sf, y = (float)ev.pos.getY() / sf;
        if (browserOpen)
            return false;
        for (const Hit &h : hits)
            if (h.param >= 0 && x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) {
                editParameter((uint32_t)h.param, true);
                stepValue(h.param, ev.delta.getY() > 0 ? 1 : -1);
                editParameter((uint32_t)h.param, false);
                return true;
            }
        return false;
    }

    bool onKeyboard(const KeyboardEvent &ev) override
    {
        if (ev.press && ev.key == kKeyEscape && browserOpen) {
            browserOpen = false;
            repaint();
            return true;
        }
        return false;
    }

private:
    // ---- values
    int soundIndex() const
    {
        const int i = (int)std::lround(vals[ond::P_SOUND]);
        return i >= 0 && i < (int)ond::sounds().size() ? i : 0;
    }
    int currentEngine() const { return ond::sounds()[(size_t)soundIndex()].engine; }

    void controlInfo(int param, ond_control_t &info) const
    {
        ond_control_info(ond::paramControls()[(size_t)param], currentEngine(), &info);
    }

    bool isEnum(int param) const
    {
        ond_control_t info;
        controlInfo(param, info);
        return info.is_enum;
    }

    int intValue(int param) const          // the firmware's value of a knob
    {
        ond_control_t info;
        controlInfo(param, info);
        if (info.scope == OND_SCOPE_EDIT)
            return ond::editValue(vals[(size_t)param], info);
        return (int)std::lround(vals[(size_t)param]);
    }

    void setValue(int param, float v, bool edit = true)
    {
        if (vals[(size_t)param] == v)
            return;
        vals[(size_t)param] = v;
        if (edit)
            editParameter((uint32_t)param, true);
        setParameterValue((uint32_t)param, v);
        if (edit)
            editParameter((uint32_t)param, false);
        repaint();
    }

    void setInt(int param, int v, bool edit = true)
    {
        ond_control_t info;
        controlInfo(param, info);
        v = v < info.min ? info.min : v > info.max ? info.max : v;
        setValue(param, info.scope == OND_SCOPE_EDIT ? ond::editNorm(v, info) : (float)v, edit);
    }

    void stepValue(int param, int dir)
    {
        ond_control_t info;
        controlInfo(param, info);
        int v = intValue(param) + dir;
        if (info.is_enum && v > info.max)
            v = info.min;                   // named values go round
        setInt(param, v, false);
    }

    void resetValue(int param)
    {
        const ond::Sound &s = ond::sounds()[(size_t)soundIndex()];
        ond_control_t info;
        const int c = ond::paramControls()[(size_t)param];
        controlInfo(param, info);
        setInt(param, info.kept ? info.def : ond_preset_value(c, s.engine, s.preset));
    }

    // a sound from the screen: SOUND, then every value of the sound as the preset has it (so the host and the
    // knobs follow); the player's own (kept: sends, level, the globals) stay as they are
    void selectSound(int i)
    {
        const ond::Sound &s = ond::sounds()[(size_t)i];
        setValue(ond::P_SOUND, (float)i);
        for (int p = 1; p < ond::paramCount(); ++p) {
            const int c = ond::paramControls()[(size_t)p];
            if (c < 0)
                continue;
            ond_control_t info;
            ond_control_info(c, s.engine, &info);
            if (info.kept || info.scope == OND_SCOPE_GLOBAL)
                continue;
            const int v = ond_preset_value(c, s.engine, s.preset);
            setValue(p, info.scope == OND_SCOPE_EDIT ? ond::editNorm(v, info) : (float)v);
        }
    }

    // ---- drawing
    void fill(float x, float y, float w, float h, Rgb c, float a = 1.f, float radius = 0.f)
    {
        beginPath();
        if (radius > 0.f)
            roundedRect(x, y, w, h, radius);
        else
            rect(x, y, w, h);
        fillColor(c.r, c.g, c.b, (int)(a * 255.f));
        NanoVG::fill();
    }

    // text in the FM-1's font (16 px a line at 1x), runs of lit pixels as rectangles; align -1 left 0 centre 1 right
    float pixelText(float x, float y, const char *s, Rgb c, float px = 1.f, int align = -1, float a = 1.f)
    {
        const felucca_font_t &f = FONT_S;
        float w = 0.f;
        for (const char *q = s; *q; ++q)
            w += f.adv[glyph(f, (uint8_t)*q)] * px;
        x = std::round(align < 0 ? x : align == 0 ? x - w / 2.f : x - w);   // whole pixels: crisp
        y = std::round(y);
        beginPath();
        float pen = x;
        for (const char *q = s; *q; ++q) {
            const uint32_t gi = glyph(f, (uint8_t)*q), bw = f.bw[gi];
            const uint8_t *gd = f.data + f.off[gi];
            for (uint32_t gy = 0; gy < f.h; ++gy) {
                uint32_t gx = 0;
                while (gx < bw) {
                    const uint32_t i = gy * bw + gx;
                    if (!((gd[i >> 3] >> (7u - (i & 7u))) & 1u)) {
                        ++gx;
                        continue;
                    }
                    uint32_t run = 1;
                    while (gx + run < bw && ((gd[(gy * bw + gx + run) >> 3] >> (7u - ((gy * bw + gx + run) & 7u))) & 1u))
                        ++run;
                    rect(pen + ((float)gx - f.pad) * px, y + (float)gy * px, (float)run * px, px);
                    gx += run;
                }
            }
            pen += f.adv[gi] * px;
        }
        fillColor(c.r, c.g, c.b, (int)(a * 255.f));
        NanoVG::fill();
        return w;
    }

    static uint32_t glyph(const felucca_font_t &f, uint32_t ch)
    {
        if (ch < f.first || ch > f.last || (ch >= 127u && ch < 160u))
            ch = '?';
        return ch - f.first;
    }

    void drawHeader()
    {
        pixelText(24.f, 12.f, "OND", TEXT, 3.f);
        const ond::Sound &s = ond::sounds()[(size_t)soundIndex()];
        const float bx = 140.f, bw = 580.f, by = 12.f, bh = 52.f;
        fill(bx, by, bw, bh, PANEL, 1.f, 4.f);
        pixelText(bx + 20.f, by + 18.f, "<", DIM, 1.f, 0);
        pixelText(bx + bw - 20.f, by + 18.f, ">", DIM, 1.f, 0);
        hits.push_back({bx, by, 40.f, bh, -10});
        hits.push_back({bx + bw - 40.f, by, 40.f, bh, -11});
        hits.push_back({bx + 40.f, by, bw - 80.f, bh, -12});
        char eng[48];
        std::snprintf(eng, sizeof eng, "%s%s", soundIndex() < ond::FAVOURITES ? "\xB7 " : "", ond_engine_name(s.engine));
        pixelText(bx + 56.f, by + 18.f, eng, DIM);
        pixelText(bx + 200.f, by + 10.f, ond_preset_name(s.engine, s.preset), browserOpen ? ACCENT : TEXT, 2.f);
        // the output: a bar that breathes with the sound
        const float mx = 744.f, mw = 232.f, my = 34.f, m = vals[(size_t)ond::meterParam()];
        const float db = m > 1e-5f ? 20.f * std::log10(m) : -100.f, k = std::fmax(0.f, std::fmin(1.f, (db + 60.f) / 60.f));
        pixelText(mx, 12.f, "OUT", DIM);
        pixelText(mx + mw, 12.f, "0.2", FAINT, 1.f, 1);
        fill(mx, my, mw, 6.f, KNOB, 1.f, 3.f);
        if (k > 0.f)
            fill(mx, my, mw * k, 6.f, db > -6.f ? ACCENT : TEXT, db > -6.f ? 1.f : 0.75f, 3.f);
    }

    void drawSection(const Section &s)
    {
        fill(s.x, s.y, s.w, s.h, PANEL, 1.f, 6.f);
        pixelText(s.x + 14.f, s.y + 10.f, s.title, DIM);
        float top = s.y + 34.f, h = s.h - 40.f;
        if (!std::strcmp(s.title, "ENVELOPE")) {
            drawEnvelope(s.x + 14.f, s.y + 40.f, s.w - 28.f, 64.f);
            top += 78.f, h -= 78.f;
        }
        const float cw = s.w / (float)s.cols, ch = h / (float)s.rows;
        const bool big = !std::strcmp(s.title, "MACROS");
        for (size_t i = 0; i < s.keys.size(); ++i) {
            const int p = ond::paramOfKey(s.keys[i]);
            if (p < 0)
                continue;
            const float cx = s.x + cw * ((float)(i % (size_t)s.cols) + 0.5f);
            const float cy = top + ch * ((float)(i / (size_t)s.cols) + 0.5f);
            const float r = big ? 38.f : std::fmin(22.f, std::fmin(cw * 0.3f, (ch - 56.f) / 2.f));
            drawKnob(p, cx, cy + 2.f, r);
            hits.push_back({cx - cw / 2.f, cy - ch / 2.f, cw, ch, p});
        }
    }

    void drawKnob(int p, float cx, float cy, float r)
    {
        ond_control_t info;
        controlInfo(p, info);
        const int v = intValue(p);
        const float a0 = 0.75f * (float)M_PI, a1 = 2.25f * (float)M_PI;
        const float k = info.max > info.min ? (float)(v - info.min) / (float)(info.max - info.min) : 0.f;
        const float av = a0 + k * (a1 - a0);
        const bool bip = info.min < 0 && info.max > 0;
        const float as = bip ? 1.5f * (float)M_PI : a0;
        const bool live = drag == p;
        const bool unused = info.label[0] == '-';

        beginPath();
        circle(cx, cy, r - 4.f);
        fillColor(KNOB.r, KNOB.g, KNOB.b, 255);
        NanoVG::fill();
        beginPath();
        arc(cx, cy, r, a0, a1, CW);
        strokeColor(FAINT.r, FAINT.g, FAINT.b, 255);
        strokeWidth(r > 30.f ? 4.f : 3.f);
        lineCap(ROUND);
        stroke();
        if (!unused && std::fabs(av - as) > 0.001f) {
            beginPath();
            arc(cx, cy, r, std::fmin(as, av), std::fmax(as, av), CW);
            strokeColor(ACCENT.r, ACCENT.g, ACCENT.b, 255);
            stroke();
        }
        if (!unused) {
            beginPath();
            moveTo(cx + std::cos(av) * r * 0.25f, cy + std::sin(av) * r * 0.25f);
            lineTo(cx + std::cos(av) * (r - 8.f), cy + std::sin(av) * (r - 8.f));
            strokeColor(TEXT.r, TEXT.g, TEXT.b, 255);
            strokeWidth(2.f);
            stroke();
        }
        pixelText(cx, cy - r - 22.f, info.label, unused ? FAINT : live ? TEXT : DIM, 1.f, 0);
        if (!unused) {
            char buf[32];
            ond_control_text(ond::paramControls()[(size_t)p], currentEngine(), v, buf, sizeof buf);
            pixelText(cx, cy + r + 6.f, buf, live ? ACCENT : TEXT, 1.f, 0);
        }
    }

    // the envelope's shape from ATK DEC SUS REL (0..127 each; the times drawn on a log-like scale)
    void drawEnvelope(float x, float y, float w, float h)
    {
        auto val = [&](const char *key) { return (float)intValue(ond::paramOfKey(key)) / 127.f; };
        const float a = val("attack"), d = val("decay"), s = val("sustain"), r = val("release");
        const float seg = w / 4.f;
        const float xa = x + seg * (0.08f + 0.92f * a), xd = xa + seg * (0.08f + 0.92f * d);
        const float xs = xd + seg * 0.8f, xr = xs + seg * (0.08f + 0.92f * r);
        const float yb = y + h, yt = y + 4.f, ys = yb - (h - 4.f) * s;
        beginPath();
        moveTo(x, yb);
        lineTo(xa, yt);
        lineTo(xd, ys);
        lineTo(xs, ys);
        lineTo(xr, yb);
        closePath();
        fillColor(ACCENT.r, ACCENT.g, ACCENT.b, 36);
        NanoVG::fill();
        beginPath();
        moveTo(x, yb);
        lineTo(xa, yt);
        lineTo(xd, ys);
        lineTo(xs, ys);
        lineTo(xr, yb);
        strokeColor(ACCENT.r, ACCENT.g, ACCENT.b, 255);
        strokeWidth(2.f);
        lineJoin(ROUND);
        stroke();
    }

    // ---- the sound browser: the engines (the baseline eight first, as OND) and their sounds
    void drawBrowser()
    {
        fill(16.f, 84.f, 968.f, 600.f, PANEL, 1.f, 6.f);
        float y = 100.f;
        browserRow(y, "OND", browserFav, -100);
        for (int e = 0; e < ond_engine_count(); ++e) {
            if (!ond_preset_count(e) || !std::strcmp(ond_engine_name(e), "SAMPLE"))
                continue;
            y += 22.f;
            browserRow(y, ond_engine_name(e), !browserFav && browserEngine == e, -200 - e);
        }
        fill(196.f, 96.f, 1.f, 576.f, EDGE);
        std::vector<int> list;
        const auto &all = ond::sounds();
        for (int i = 0; i < (int)all.size(); ++i)
            if (browserFav ? i < ond::FAVOURITES : all[(size_t)i].engine == browserEngine && i >= ond::FAVOURITES)
                list.push_back(i);
        if (!browserFav)                     // the engine's sounds in its own order (the eight are favourites too)
            for (int i = 0; i < ond::FAVOURITES; ++i)
                if (all[(size_t)i].engine == browserEngine)
                    list.push_back(i);
        std::sort(list.begin(), list.end(), [&](int a, int b) {
            return browserFav ? a < b : all[(size_t)a].preset < all[(size_t)b].preset;
        });
        for (size_t k = 0; k < list.size(); ++k) {
            const ond::Sound &s = all[(size_t)list[k]];
            const float bx = 216.f + 256.f * (float)(k / 22), byy = 100.f + 25.f * (float)(k % 22);
            const bool cur = list[k] == soundIndex();
            if (cur)
                fill(bx - 6.f, byy - 4.f, 244.f, 24.f, ACCENT, 0.16f, 3.f);
            pixelText(bx, byy, ond_preset_name(s.engine, s.preset), cur ? ACCENT : TEXT);
            hits.push_back({bx - 6.f, byy - 4.f, 244.f, 25.f, -1000 - list[k]});
        }
        pixelText(984.f - 14.f, 656.f, "ESC / CLICK THE NAME TO CLOSE", FAINT, 1.f, 1);
    }

    void browserRow(float y, const char *name, bool on, int hit)
    {
        if (on)
            fill(24.f, y - 4.f, 164.f, 22.f, ACCENT, 0.16f, 3.f);
        pixelText(36.f, y, name, on ? ACCENT : DIM);
        hits.push_back({24.f, y - 4.f, 164.f, 22.f, hit});
    }

    bool browserClick(float x, float y)
    {
        for (const Hit &h : hits) {
            if (x < h.x || x >= h.x + h.w || y < h.y || y >= h.y + h.h)
                continue;
            if (h.param == -12) {
                browserOpen = false;
            } else if (h.param == -100) {
                browserFav = true;
            } else if (h.param <= -200 && h.param > -1000) {
                browserFav = false;
                browserEngine = -200 - h.param;
            } else if (h.param <= -1000) {
                selectSound(-1000 - h.param);
                browserOpen = false;
            } else if (h.param == -10 || h.param == -11) {
                const int n = (int)ond::sounds().size();
                selectSound((soundIndex() + (h.param == -11 ? 1 : n - 1)) % n);
            }
            repaint();
            return true;
        }
        return true;
    }

    std::vector<float> vals;
    std::vector<Hit> hits;
    int drag = -1, lastParam = -1;
    float dragY = 0.f, dragStart = 0.f;
    bool moved = false;
    uint lastClick = 0;
    bool browserOpen = false, browserFav = true;
    int browserEngine = 7;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OndUI)
};

UI *createUI() { return new OndUI(); }

END_NAMESPACE_DISTRHO
