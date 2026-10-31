// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// Ond's screen (DPF, NanoVG): the sound browser, the four macros, the engine's own EDIT knobs, the envelope,
// character, play and space sections. Every label and value is the FM-1's (the firmware's names and
// param_format, ond_control_text), drawn in the FM-1's own pixel font (Terminus, build/gen/felucca_font.h).
#include "DistrhoUI.hpp"
#include "OndPresets.hpp"
#include "OndLink.hpp"
#include "OndShared.hpp"
#include "ond_grooves.h"
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
    int param;                     // >= 0 a knob; -10 previous, -11 next, -12 the browser, -20 SAVE, -21 SAVE AS;
                                   // the browser: -100 OND, -101 MINE, -200-e an engine, -1000-i a sound,
                                   // -2000-k my preset k, -3000-k delete it, -30 RENAME, -31 the folder;
                                   // -22 the BRUDD view, -4000-i breaker i's pad, -23 LEARN; -24 the LAYER
                                   // view, -25/-26 the layer's previous / next sound, -27 its browser;
                                   // -5000-k hit k's pad, -28 the hits' folder, -29 RELOAD them; -32 the BEAT
                                   // view, -33 NEW (another groove of the feel), -34 PLAY, -6000-k part k in / out
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
        mine = ond::listPresets();
        pads = ond::PadMap::parse(ond::loadPadsText().c_str());
        hitNames = ond::listHits();
        scanKit();
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= vals.size() || vals[index] == value)
            return;
        vals[index] = value;
        if ((int)index == ond::midiInParam() && learnArmed >= 0) {   // LEARN: the pad just hit is this breaker's
            pads.assign(learnArmed, (int)value & 255);
            learnArmed = -1;
            storePads();
        }
        repaint();
    }

    void stateChanged(const char *key, const char *value) override
    {
        if (!std::strcmp(key, "pads")) {
            if (value[0])
                pads = ond::PadMap::parse(value);
            repaint();
            return;
        }
        if (std::strcmp(key, "preset"))
            return;
        userName = value;                  // (a Live set opened: the preset it had; whether it was changed since
        modified = false;                  // is not known: shown as saved)
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
                if (view == 0 || s.y < 300.f)
                    drawSection(s);
        if (view == 1 && !browserOpen)
            drawBrudd();
        if (view == 2 && !browserOpen)
            drawLayer();
        if (view == 3 && !browserOpen)
            drawBeat();
        if (view == 4 && !browserOpen)
            drawLink();
        if (drag >= 0)
            drawDragReadout();
        restore();
    }

    bool onMouse(const MouseEvent &ev) override
    {
        const float sf = (float)getWidth() / W;
        const float x = (float)ev.pos.getX() / sf, y = (float)ev.pos.getY() / sf;
        if (ev.button != 1)
            return false;
        if (!ev.press) {
            if (padDown >= 0) {                      // a pad let go: the groove comes back
                setValue(ond::breakerParam(), 0.f, false);
                editParameter((uint32_t)ond::breakerParam(), false);
                padDown = -1;
                repaint();
            }
            if (hitDown >= 0) {
                setValue(ond::hitParam(), 0.f, false);
                editParameter((uint32_t)ond::hitParam(), false);
                hitDown = -1;
                repaint();
            }
            if (drag >= 0) {
                if (!moved && isEnum(drag)) {
                    stepValue(drag, 1);              // a click on a named value: the next one
                    touch();
                }
                editParameter((uint32_t)drag, false);
                drag = -1;
                repaint();
            }
            return true;
        }
        if (renaming)
            finishRename(true);                      // a click anywhere: the name as typed
        if (browserOpen)
            return browserClick(x, y);
        for (const Hit &h : hits) {
            if (x < h.x || x >= h.x + h.w || y < h.y || y >= h.y + h.h)
                continue;
            if (h.param == -10 || h.param == -11) {
                step(h.param == -11 ? 1 : -1);
            } else if (h.param == -12) {
                openBrowser();
            } else if (h.param == -20) {
                saveCurrent();
            } else if (h.param == -21) {
                saveAs();
            } else if (h.param == -33) {
                newGroove();
            } else if (h.param <= -6000 && h.param > -7000) {   // a part of the kit in / out of the groove
                const int p = ond::paramOfKey(PART_KEYS[-6000 - h.param]);
                if (p >= 0) {
                    setValue(p, vals[(size_t)p] >= 0.5f ? 0.f : 1.f);
                    touch();
                }
            } else if (h.param == -37) {
                saveMidi();
            } else if (h.param == -38) {
                ond::openBeatsFolder();
            } else if (h.param == -35) {
                ond::openKitFolder();
            } else if (h.param == -36) {
                scanKit();
                hitNames = ond::listHits();
                char n[16];
                std::snprintf(n, sizeof n, "%u", ++hitsReloads);
                setState("hits", n);                 // (the plugin reads the hits and the kit again)
            } else if (h.param == -34) {
                setValue(ond::playParam(), vals[(size_t)ond::playParam()] >= 0.5f ? 0.f : 1.f);
            } else if (h.param == -22 || h.param == -24 || h.param == -32 || h.param == -39) {
                const int v = h.param == -22 ? 1 : h.param == -24 ? 2 : h.param == -32 ? 3 : 4;
                view = view == v ? 0 : v;
                learning = false, learnArmed = -1;
            } else if (h.param == -25 || h.param == -26) {
                const int n = (int)ond::sounds().size();
                setValue(ond::layerSoundParam(), (float)((layerIndex() + (h.param == -26 ? 1 : n - 1)) % n));
                touch();
            } else if (h.param == -27) {
                openBrowser(true);
            } else if (h.param == -23) {
                learning = !learning;
                learnArmed = -1;
            } else if (h.param == -28) {
                ond::openHitsFolder();
            } else if (h.param == -29) {
                hitNames = ond::listHits();
                char n[16];
                std::snprintf(n, sizeof n, "%u", ++hitsReloads);
                setState("hits", n);                 // (the plugin reads the folder again)
            } else if (h.param <= -4000 && learning) {   // LEARN: arm this breaker or hit; again: forget its pad
                const int i = h.param <= -5000 ? ond::NBREAK + (-5000 - h.param) : -4000 - h.param;
                if (learnArmed == i) {
                    pads.assign(i, -1);
                    learnArmed = -1;
                    storePads();
                } else {
                    learnArmed = i;
                }
            } else if (h.param <= -5000) {
                hitDown = -5000 - h.param;           // a hit: heard on the press
                editParameter((uint32_t)ond::hitParam(), true);
                setValue(ond::hitParam(), (float)(hitDown + 1), false);
            } else if (h.param <= -4000) {
                padDown = -4000 - h.param;           // held while the button is
                editParameter((uint32_t)ond::breakerParam(), true);
                setValue(ond::breakerParam(), (float)(padDown + 1), false);
            } else if (h.param >= 0) {
                if (ev.time - lastClick < 350 && lastParam == h.param) {
                    resetValue(h.param);             // double click: as the sound has it
                    touch();
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
        if (std::fabs(dy) > 2.f && !moved) {
            moved = true;
            touch();
        }
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
                touch();
                return true;
            }
        return false;
    }

    bool onKeyboard(const KeyboardEvent &ev) override
    {
        if (!ev.press)
            return renaming;
        if (renaming) {
            if (ev.key == kKeyEnter || ev.key == kKeyEscape)
                finishRename(ev.key == kKeyEnter);
            else if (ev.key == kKeyBackspace) {
                if (fresh)
                    editName.clear();
                else if (!editName.empty())
                    editName.pop_back();
                fresh = false;
                repaint();
            }
            return true;                    // typing a name: the keys are ours, not Live's
        }
        if (ev.key == kKeyEscape && browserOpen) {
            browserOpen = false;
            repaint();
            return true;
        }
        return false;
    }

    bool onCharacterInput(const CharacterInputEvent &ev) override
    {
        if (!renaming)
            return false;
        const uint c = ev.character;
        if (c >= 32u && c < 127u && c != '=') {
            if (fresh)
                editName.clear();               // the first key replaces the offered name
            fresh = false;
            if (editName.size() < 24u)
                editName += (char)c;
            repaint();
        }
        return true;
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

    bool learnedElsewhere(int note) const  // a default note taken by LEARN for another pad
    {
        for (int c : pads.code)
            if (c == note)
                return true;
        return false;
    }

    void scanKit()                         // which lanes have a recording of yours
    {
        for (int k = 0; k < 16; ++k) {
            std::vector<float> l, r;
            kitHas[k] = ond::loadKitLane(k, l, r);
        }
    }

    void storePads()                       // to the plugin (the set keeps it) and for every set
    {
        const std::string t = pads.text();
        setState("pads", t.c_str());
        ond::savePadsText(t);
        repaint();
    }

    // ---- the player's presets (OndPresets.hpp)
    void touch()                           // a knob changed by hand: the preset loaded is not what sounds
    {
        if (!userName.empty() && !modified) {
            modified = true;
            repaint();
        }
    }

    void setUser(const std::string &name)
    {
        userName = name;
        modified = false;
        setState("preset", name.c_str());
    }

    int mineIndex() const
    {
        for (size_t k = 0; k < mine.size(); ++k)
            if (mine[k] == userName)
                return (int)k;
        return -1;
    }

    void step(int dir)                     // < >: through my presets while one is loaded, else the sounds
    {
        const int k = mineIndex();
        if (k >= 0 && !mine.empty()) {
            loadUser(mine[(size_t)((k + dir + (int)mine.size()) % (int)mine.size())]);
            return;
        }
        const int n = (int)ond::sounds().size();
        selectSound((soundIndex() + dir + n) % n);
    }

    void openBrowser(bool forLayer = false)
    {
        mine = ond::listPresets();         // (files added or removed outside since)
        browserOpen = true;
        browserLayer = forLayer;
        const int i = forLayer ? layerIndex() : soundIndex();
        browserEngine = ond::sounds()[(size_t)i].engine;
        browserGroup = !forLayer && mineIndex() >= 0 ? 1 : i < ond::FAVOURITES ? 0 : 2;
        deleteArmed = -1;
    }

    // what sounds now, as a preset: the sound, then every control's value (the firmware's)
    ond::UserPreset capture(const std::string &name) const
    {
        ond::UserPreset p;
        const ond::Sound &s = ond::sounds()[(size_t)soundIndex()];
        p.name = name;
        p.engine = ond_engine_name(s.engine);
        p.preset = ond_preset_name(s.engine, s.preset);
        const ond::Sound &ls = ond::sounds()[(size_t)layerIndex()];
        p.layerEngine = ond_engine_name(ls.engine);
        p.layerPreset = ond_preset_name(ls.engine, ls.preset);
        for (int i = 1; i < ond::paramCount(); ++i) {
            const int c = ond::paramControls()[(size_t)i];
            if (c < 0)
                continue;
            ond_control_t info;
            ond_control_info(c, s.engine, &info);
            p.values[info.key] = intValue(i);
        }
        return p;
    }

    void saveCurrent()                     // SAVE: over the preset loaded (a factory sound: as a new one)
    {
        if (userName.empty()) {
            saveAs();
            return;
        }
        if (ond::savePreset(capture(userName)))
            modified = false;
        repaint();
    }

    void saveAs()                          // SAVE AS: a new preset, named after the sound; type to rename it
    {
        const ond::Sound &s = ond::sounds()[(size_t)soundIndex()];
        const std::string base = userName.empty() ? ond_preset_name(s.engine, s.preset) : userName;
        const std::string name = ond::freeName(base);
        if (!ond::savePreset(capture(name)))
            return;
        mine = ond::listPresets();
        setUser(name);
        startRename();
    }

    void startRename()
    {
        if (userName.empty())
            return;
        browserOpen = false;
        renaming = true;
        fresh = true;
        editName = userName;
        repaint();
    }

    void finishRename(bool keep)
    {
        renaming = false;
        std::string to = editName;
        while (!to.empty() && to.back() == ' ')
            to.pop_back();
        if (keep && !to.empty() && to != userName && ond::renamePreset(userName, to)) {
            mine = ond::listPresets();
            const bool was = modified;
            setUser(to);
            modified = was;
        }
        repaint();
    }

    void loadUser(const std::string &name)
    {
        ond::UserPreset p;
        if (!ond::loadPreset(name, p))
            return;
        int found = -1;                    // its sound, by names (indices may move between versions)
        const auto &all = ond::sounds();
        for (int i = 0; i < (int)all.size() && found < 0; ++i)
            if (p.engine == ond_engine_name(all[(size_t)i].engine) &&
                p.preset == ond_preset_name(all[(size_t)i].engine, all[(size_t)i].preset))
                found = i;
        selectSound(found >= 0 ? found : 0);
        if (!p.layerEngine.empty())        // the layer's sound, by names
            for (int i = 0; i < (int)all.size(); ++i)
                if (p.layerEngine == ond_engine_name(all[(size_t)i].engine) &&
                    p.layerPreset == ond_preset_name(all[(size_t)i].engine, all[(size_t)i].preset)) {
                    setValue(ond::layerSoundParam(), (float)i);
                    break;
                }
        const ond::Sound &s = all[(size_t)(found >= 0 ? found : 0)];
        for (int i = 1; i < ond::paramCount(); ++i) {
            const int c = ond::paramControls()[(size_t)i];
            if (c < 0)
                continue;
            ond_control_t info;
            ond_control_info(c, s.engine, &info);
            const auto v = p.values.find(info.key);
            if (v != p.values.end())
                setInt(i, v->second);
        }
        setUser(name);
        repaint();
    }

    // a sound from the screen: SOUND, then every value of the sound as the preset has it (so the host and the
    // knobs follow); the player's own (kept: sends, level, the globals) stay as they are
    void selectSound(int i)
    {
        if (!userName.empty())
            setUser("");                   // a factory sound now (loadUser names its own after this)
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

    void button(float x, float y, float w, float h, const char *label, bool on, int hit)
    {
        fill(x, y, w, h, on ? ACCENT : KNOB, on ? 0.18f : 1.f, 4.f);
        pixelText(x + w / 2.f, y + h / 2.f - 8.f, label, on ? ACCENT : DIM, 1.f, 0);
        hits.push_back({x, y, w, h, hit});
    }

    void drawHeader()
    {
        pixelText(24.f, 12.f, "OND", TEXT, 3.f);
        const ond::Sound &s = ond::sounds()[(size_t)soundIndex()];
        const float bx = 140.f, bw = 430.f, by = 12.f, bh = 52.f;
        fill(bx, by, bw, bh, PANEL, 1.f, 4.f);
        pixelText(bx + 18.f, by + 18.f, "<", DIM, 1.f, 0);
        pixelText(bx + bw - 18.f, by + 18.f, ">", DIM, 1.f, 0);
        hits.push_back({bx, by, 36.f, bh, -10});
        hits.push_back({bx + bw - 36.f, by, 36.f, bh, -11});
        hits.push_back({bx + 36.f, by, bw - 72.f, bh, -12});
        char line[96];
        if (!userName.empty())             // mine: its name, the sound under it small
            std::snprintf(line, sizeof line, "MINE \xB7 %s %s", ond_engine_name(s.engine), ond_preset_name(s.engine, s.preset));
        else
            std::snprintf(line, sizeof line, "%s%s", soundIndex() < ond::FAVOURITES ? "OND \xB7 " : "", ond_engine_name(s.engine));
        pixelText(bx + 44.f, by + 2.f, line, DIM);
        std::string name = renaming ? editName : !userName.empty() ? userName : ond_preset_name(s.engine, s.preset);
        if (!renaming && modified)
            name += "*";
        const Rgb nc = renaming ? (fresh ? DIM : TEXT) : browserOpen ? ACCENT : TEXT;
        const float tw = pixelText(bx + 44.f, by + 19.f, name.c_str(), nc, 2.f);
        if (renaming)                      // the cursor
            fill(bx + 44.f + tw + 2.f, by + 19.f, 3.f, 30.f, ACCENT);
        button(578.f, by, 54.f, bh, "SAVE", !userName.empty() && modified, -20);
        button(636.f, by, 72.f, bh, "SAVE AS", false, -21);
        button(712.f, by, 64.f, bh, "LAYER", view == 2 || layerOn(), -24);
        button(780.f, by, 64.f, bh, "BRUDD", view == 1, -22);
        button(848.f, by, 62.f, bh, "BEAT", view == 3 || beatOn(), -32);
        button(914.f, by, 62.f, bh, "LINK", view == 4, -39);
        // the output: a line under the name that breathes with the sound
        const float m = vals[(size_t)ond::meterParam()];
        const float db = m > 1e-5f ? 20.f * std::log10(m) : -100.f, k = std::fmax(0.f, std::fmin(1.f, (db + 60.f) / 60.f));
        if (k > 0.f)
            fill(bx + 4.f, by + bh - 3.f, (bw - 8.f) * k, 2.f, db > -6.f ? ACCENT : TEXT, db > -6.f ? 1.f : 0.5f, 1.f);
        pixelText(24.f, 60.f, "0.22", FAINT);
        if (renaming)
            pixelText(bx + bw, by + bh + 6.f, "TYPE A NAME, ENTER", FAINT, 1.f, 1);
    }

    // Brudd's breakers (docs/rhythm-breaks.md): hold a pad, the whole mix breaks; let go, the groove is back where
    // it is now. The MPK's bank B pads (channel 10, notes 44..51) are marked with their number
    void drawBrudd()
    {
        const float x0 = 16.f, y0 = 336.f, w = 968.f, h = 348.f;
        fill(x0, y0, w, h, PANEL, 1.f, 6.f);
        pixelText(x0 + 14.f, y0 + 10.f, "BRUDD", DIM);
        button(x0 + 80.f, y0 + 4.f, 70.f, 26.f, "LEARN", learning, -23);
        const char *hint = !learning ? "HOLD A PAD. LET GO: BACK IN THE GROOVE"
                           : learnArmed < 0 ? "CLICK A BREAKER, THEN HIT ITS PAD. CLICK IT TWICE: FORGET"
                                            : "NOW HIT A PAD ON THE MPK";
        pixelText(x0 + 164.f, y0 + 10.f, hint, learning ? ACCENT : FAINT);
        {   // what arrives: so a pad that does nothing can be seen sending
            const int v = (int)std::lround(vals[(size_t)ond::midiInParam()]);
            if (v) {
                char in[48];
                std::snprintf(in, sizeof in, "IN: CH %d %s", (v / 256) % 16 + 1, ond::codeName(v & 255).c_str());
                pixelText(x0 + w - 14.f, y0 + 10.f, in, DIM, 1.f, 1);
            }
        }
        const int n = ond_breaker_count(), playing = (int)std::lround(vals[(size_t)ond::breakerOutParam()]) - 1;
        const float side = 130.f;           // CATCH and LAND on the right
        const float gap = 8.f, pw = (w - 28.f - side - 7.f * gap) / 8.f, ph = (h - 50.f - 2.f * gap) / 3.f;
        static const char *const KEYS[2] = {"catch", "land"};
        for (int k = 0; k < 2; ++k) {
            const int p = ond::paramOfKey(KEYS[k]);
            const float cx = x0 + w - 14.f - side / 2.f + 6.f, cy = y0 + 30.f + ph * ((float)k + 0.5f) + gap * (float)k;
            if (p < 0)
                continue;
            drawKnob(p, cx, cy + 8.f, 20.f);
            hits.push_back({cx - side / 2.f, cy - ph / 2.f, side, ph, p});
        }
        for (int i = 0; i < n; ++i) {
            const float px = x0 + 14.f + (pw + gap) * (float)(i % 8), py = y0 + 36.f + (ph + gap) * (float)(i / 8);
            const bool on = playing == i || padDown == i, armed = learning && learnArmed == i;
            fill(px, py, pw, ph, on ? ACCENT : KNOB, on ? (playing == i ? 0.85f : 0.4f) : 1.f, 6.f);
            if (armed) {
                beginPath();
                roundedRect(px + 1.f, py + 1.f, pw - 2.f, ph - 2.f, 6.f);
                strokeColor(ACCENT.r, ACCENT.g, ACCENT.b, 255);
                strokeWidth(2.f);
                stroke();
            }
            pixelText(px + pw / 2.f, py + ph / 2.f - 8.f, ond_breaker_name(i), on ? BG : TEXT, 1.f, 0);
            std::string tag = ond::codeName(pads.code[i]);   // its pad: learned, else its own note (0..15)
            if (tag.empty() && ond::defaultNoteOf(i) >= 0 && !learnedElsewhere(ond::defaultNoteOf(i)))
                tag = ond::codeName(ond::defaultNoteOf(i));
            if (!tag.empty())
                pixelText(px + 10.f, py + 8.f, tag.c_str(), on ? BG : armed || learning ? ACCENT : FAINT);
            hits.push_back({px, py, pw, ph, -4000 - i});
        }
        for (int k = 0; k < 8; ++k) {            // the hits: the folder's first eight, by name
            const float px = x0 + 14.f + (pw + gap) * (float)k, py = y0 + 36.f + (ph + gap) * 2.f;
            const bool armed = learning && learnArmed == ond::NBREAK + k, down = hitDown == k;
            const bool has = k < (int)hitNames.size();
            fill(px, py, pw, ph, down ? ACCENT : PANEL, down ? 0.6f : 1.f, 6.f);
            beginPath();
            roundedRect(px + 1.f, py + 1.f, pw - 2.f, ph - 2.f, 6.f);
            strokeColor(armed ? ACCENT.r : EDGE.r, armed ? ACCENT.g : EDGE.g, armed ? ACCENT.b : EDGE.b, 255);
            strokeWidth(armed ? 2.f : 1.f);
            stroke();
            std::string name = has ? hitNames[(size_t)k] : "-";
            if (name.size() > 10)
                name = name.substr(0, 9) + ".";
            pixelText(px + pw / 2.f, py + ph / 2.f - 4.f, name.c_str(), has ? TEXT : FAINT, 1.f, 0);
            std::string tag = ond::codeName(pads.code[ond::NBREAK + k]);
            if (tag.empty() && !learnedElsewhere(ond::defaultNoteOf(ond::NBREAK + k)))
                tag = ond::codeName(ond::defaultNoteOf(ond::NBREAK + k));
            pixelText(px + 8.f, py + 6.f, tag.empty() ? "HIT" : tag.c_str(), armed || learning ? ACCENT : FAINT);
            hits.push_back({px, py, pw, ph, -5000 - k});
        }
        button(x0 + w - 14.f - side + 4.f, y0 + 36.f + (ph + gap) * 2.f, side - 4.f, ph / 2.f - 3.f, "HITS FOLDER", false, -28);
        button(x0 + w - 14.f - side + 4.f, y0 + 36.f + (ph + gap) * 2.f + ph / 2.f + 3.f, side - 4.f, ph / 2.f - 3.f,
               "RELOAD", false, -29);
    }

    // the layer (Clipper's LAYER): a second sound under the first (LAYER) or below a split (SPLIT), its octave,
    // level, pan and filter: a drone under the organ, or one hand each
    bool layerOn() const
    {
        const int p = ond::paramOfKey("layer_mode");
        return p >= 0 && std::lround(vals[(size_t)p]) != 0;
    }
    int layerIndex() const
    {
        const int i = (int)std::lround(vals[(size_t)ond::layerSoundParam()]);
        return i >= 0 && i < (int)ond::sounds().size() ? i : 0;
    }

    void drawLayer()
    {
        const float x0 = 16.f, y0 = 336.f, w = 968.f, h = 348.f;
        fill(x0, y0, w, h, PANEL, 1.f, 6.f);
        pixelText(x0 + 14.f, y0 + 10.f, "LAYER", DIM);
        pixelText(x0 + w - 14.f, y0 + 10.f, "A SECOND SOUND: UNDER EVERY NOTE (LAYER), OR BELOW THE SPLIT (SPLIT)", FAINT, 1.f, 1);
        const ond::Sound &s = ond::sounds()[(size_t)layerIndex()];
        const float bx = x0 + 14.f, by = y0 + 40.f, bw = 420.f, bh = 52.f;   // its sound, as the header's
        fill(bx, by, bw, bh, KNOB, 1.f, 4.f);
        pixelText(bx + 18.f, by + 18.f, "<", DIM, 1.f, 0);
        pixelText(bx + bw - 18.f, by + 18.f, ">", DIM, 1.f, 0);
        hits.push_back({bx, by, 36.f, bh, -25});
        hits.push_back({bx + bw - 36.f, by, 36.f, bh, -26});
        hits.push_back({bx + 36.f, by, bw - 72.f, bh, -27});
        pixelText(bx + 44.f, by + 2.f, ond_engine_name(s.engine), DIM);
        pixelText(bx + 44.f, by + 19.f, ond_preset_name(s.engine, s.preset), layerOn() ? TEXT : DIM, 2.f);
        static const char *const KEYS[] = {"layer_mode", "layer_oct", "layer_split", "layer_level", "layer_pan",
                                           "layer_filter"};
        const float cw = (w - 28.f) / 6.f, cy = y0 + 220.f;
        for (int i = 0; i < 6; ++i) {
            const int p = ond::paramOfKey(KEYS[i]);
            if (p < 0)
                continue;
            const float cx = x0 + 14.f + cw * ((float)i + 0.5f);
            drawKnob(p, cx, cy, 30.f);
            hits.push_back({cx - cw / 2.f, cy - 70.f, cw, 140.f, p});
        }
    }

    static constexpr const char *N_FEEL_UI[5] = {"off", "hiphop", "funk", "soul", "jazz"};
    static constexpr const char *PART_KEYS[7] = {"beat_kick", "beat_snare", "beat_rim", "beat_hats", "beat_cymbals",
                                                 "beat_toms", "beat_perc"};
    static constexpr const char *PART_NAMES[7] = {"KICK", "SNARE", "RIM", "HATS", "CYMBALS", "TOMS", "PERC"};

    // BEAT (task 015): a drummer's groove (ond_grooves.h) on SLOOP's kits, while Live plays
    bool beatOn() const
    {
        const int p = ond::paramOfKey("beat_feel");
        return p >= 0 && std::lround(vals[(size_t)p]) != 0;
    }
    int feelCount(int feel) const
    {
        int n = 0;
        for (int i = 0; i < OND_NGROOVES; ++i)
            n += OND_GROOVES[i].feel == feel - 1;
        return n;
    }
    const ond_groove_t *grooveNow() const
    {
        const int pf = ond::paramOfKey("beat_feel"), pg = ond::paramOfKey("beat_groove");
        const int feel = pf >= 0 ? (int)std::lround(vals[(size_t)pf]) : 0, n = feelCount(feel);
        if (feel < 1 || !n || pg < 0)
            return nullptr;
        int k = ((int)std::lround(vals[(size_t)pg]) - 1) % n;
        for (int i = 0; i < OND_NGROOVES; ++i)
            if (OND_GROOVES[i].feel == feel - 1 && !k--)
                return &OND_GROOVES[i];
        return nullptr;
    }
    void saveMidi()                        // SAVE MIDI: 8 bars as they play, to drag into a Live clip
    {
        static uint8_t vel[8][16][16];
        static int8_t micro[8][16];
        const ond_groove_t *g = grooveNow();
        if (!g)
            return;
        static const char *const KEYS[12] = {"beat_feel", "beat_groove", "beat_drunk", "beat_bars", "beat_fill",
                                             "beat_kick", "beat_snare", "beat_rim", "beat_hats", "beat_cymbals",
                                             "beat_toms", "beat_perc"};
        int v[12];
        for (int i = 0; i < 12; ++i) {           // the screen's values (the host's), not the engine's copy
            const int p = ond::paramOfKey(KEYS[i]);
            v[i] = p >= 0 ? (int)std::lround(vals[(size_t)p]) : 0;
        }
        for (int b = 0; b < 8; ++b)
            ond_beat_bar(v, b, vel[b], micro[b]);
        char name[64];
        std::snprintf(name, sizeof name, "%s %u drummer%u", N_FEEL_UI[(g->feel + 1) % 5], (unsigned)(grooveIndex()),
                      (unsigned)g->drummer);
        savedMidi = ond::saveBeatMidi(name, 8, vel, micro, getTempo());
        repaint();
    }
    int grooveIndex() const
    {
        const int pg = ond::paramOfKey("beat_groove");
        return pg >= 0 ? (int)std::lround(vals[(size_t)pg]) : 1;
    }
    double getTempo() const { return 90.0; }   // (the file's tempo meta: Live takes the clip at its own)

    void newGroove()                       // NEW: another of the feel's grooves (a feel off: HIP HOP first)
    {
        const int pf = ond::paramOfKey("beat_feel"), pg = ond::paramOfKey("beat_groove");
        if (pf < 0 || pg < 0)
            return;
        int feel = (int)std::lround(vals[(size_t)pf]);
        if (feel < 1)
            setValue(pf, (float)(feel = 1));
        const int n = feelCount(feel), now = (int)std::lround(vals[(size_t)pg]);
        if (n < 2)
            return;
        int g = now;
        while (g == now)
            g = 1 + std::rand() % n;
        setValue(pg, (float)g);
        touch();
    }

    void drawBeat()
    {
        const float x0 = 16.f, y0 = 336.f, w = 968.f, h = 348.f;
        fill(x0, y0, w, h, PANEL, 1.f, 6.f);
        pixelText(x0 + 14.f, y0 + 10.f, "BEAT", DIM);
        {   // where its time comes from: Live's PLAY, or its own
            const int tr = (int)std::lround(vals[(size_t)ond::transportParam()]);
            const bool own = vals[(size_t)ond::playParam()] >= 0.5f;
            const char *st = !beatOn() ? "FEEL OFF: NEW PICKS ONE"
                             : tr == 1 ? "PLAYING WITH LIVE"
                             : tr == 2 ? "PLAYING (LIVE STOPPED: OND'S OWN TIME)"
                                       : "WAITING: PRESS PLAY HERE OR IN LIVE";
            pixelText(x0 + w - 14.f, y0 + 10.f, st, tr && beatOn() ? ACCENT : DIM, 1.f, 1);
            button(x0 + w - 14.f - 250.f, y0 + 40.f, 120.f, 34.f, own ? "STOP" : "PLAY", own, -34);
            button(x0 + w - 14.f - 390.f, y0 + 40.f, 130.f, 34.f, "SAVE MIDI", false, -37);
            button(x0 + w - 14.f - 530.f, y0 + 40.f, 130.f, 34.f, "BEATS FOLDER", false, -38);
        }
        const ond_groove_t *g = grooveNow();
        char info[96];
        if (g)
            std::snprintf(info, sizeof info, "DRUMMER %u, %u BPM (%s), %u BARS OF THEIR PLAYING", (unsigned)g->drummer,
                          (unsigned)g->bpm, g->style, (unsigned)g->nbars);
        else
            std::snprintf(info, sizeof info, "OFF: PICK A FEEL, OR PRESS NEW");
        pixelText(x0 + 14.f, y0 + 48.f, info, g ? TEXT : DIM, 1.f);
        if (!savedMidi.empty())                 // (the last SAVE MIDI: where to drag it from)
            pixelText(x0 + 14.f, y0 + 72.f, ("SAVED: BEATS\\" + savedMidi + ".MID - DRAG IT INTO LIVE").c_str(), ACCENT);
        button(x0 + w - 14.f - 120.f, y0 + 40.f, 120.f, 34.f, "NEW", false, -33);
        static const char *const KEYS[] = {"beat_feel", "beat_groove", "beat_bars", "beat_fill", "beat_drunk",
                                           "beat_kit", "beat_tone", "beat_dust", "beat_pump", "beat_level"};
        {   // the parts of the kit: lit in the groove, dark out of it
            const float gap = 8.f, by = y0 + 96.f, bw = (w - 28.f - 136.f - 6.f * gap) / 7.f;   // (all seven inside)
            pixelText(x0 + 14.f, by + 8.f, "IN THE GROOVE:", DIM);
            for (int k = 0; k < 7; ++k) {
                const int p = ond::paramOfKey(PART_KEYS[k]);
                const bool in = p >= 0 && vals[(size_t)p] >= 0.5f;
                button(x0 + 150.f + (bw + gap) * (float)k, by, bw, 32.f, PART_NAMES[k], in, -6000 - k);   // (to x0 + w - 14)
            }
        }
        const float cw = (w - 28.f) / 10.f, cy = y0 + 236.f;
        for (int i = 0; i < 10; ++i) {
            const int p = ond::paramOfKey(KEYS[i]);
            if (p < 0)
                continue;
            const float cx = x0 + 14.f + cw * ((float)i + 0.5f);
            drawKnob(p, cx, cy, 30.f);
            hits.push_back({cx - cw / 2.f, cy - 70.f, cw, 140.f, p});
        }
        {   // YOUR KIT: the lanes with a recording of yours in Documents/Ond/Kit
            std::string mine;
            for (int k = 0; k < 16; ++k)
                if (kitHas[k])
                    mine += (mine.empty() ? "" : " ") + std::string(ond::KIT_FILES[k]);
            pixelText(x0 + 14.f, y0 + h - 70.f, mine.empty() ? "YOUR KIT: NONE YET (KICK.WAV, SNARE.WAV, HAT.WAV ... IN THE KIT FOLDER)"
                                                            : ("YOUR KIT: " + mine).c_str(), mine.empty() ? FAINT : TEXT);
            button(x0 + w - 14.f - 250.f, y0 + h - 78.f, 120.f, 28.f, "KIT FOLDER", false, -35);
            button(x0 + w - 14.f - 120.f, y0 + h - 78.f, 120.f, 28.f, "RELOAD", false, -36);
        }
        pixelText(x0 + 14.f, y0 + h - 44.f, "OUT AS MIDI NOTES 16-31: RECORD THEM ON A TRACK, 'MIDI FROM: OND'. A CLIP'S 16-31 PLAY THE KIT.", FAINT);
        pixelText(x0 + 14.f, y0 + h - 26.f, "GROOVE MIDI DATASET (GOOGLE MAGENTA, CC BY 4.0). KITS: SLOOP'S.", FAINT);
    }

    // LINK: the Onds of this set hear each other (OndLink.hpp)
    void drawLink()
    {
        const float x0 = 16.f, y0 = 336.f, w = 968.f, h = 348.f;
        auto &hub = ond::LinkHub::get();
        fill(x0, y0, w, h, PANEL, 1.f, 6.f);
        pixelText(x0 + 14.f, y0 + 10.f, "LINK", DIM);
        char line[96];
        std::snprintf(line, sizeof line, "ONDS IN THIS SET: %d", hub.count());
        pixelText(x0 + w - 14.f, y0 + 10.f, line, hub.count() > 1 ? ACCENT : DIM, 1.f, 1);
        int beats = 0, low = 128;
        for (const auto &s : hub.slot) {        // what the others offer now (this Ond's own slot counts too: rough)
            beats += s.owner.load() && s.beatOn.load();
            for (int wd = 0; wd < 4; ++wd)
                for (int b = 0; b < 32; ++b)
                    if ((s.notes[wd].load() >> b) & 1u && wd * 32 + b < low)
                        low = wd * 32 + b;
        }
        static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        std::snprintf(line, sizeof line, "BEATS PLAYING: %d   LOWEST KEY HELD IN ANY OND: %s", beats,
                      low < 128 ? (std::string(NN[low % 12]) + std::to_string(low / 12 - 2)).c_str() : "-");
        pixelText(x0 + 14.f, y0 + 40.f, line, TEXT);
        pixelText(x0 + 14.f, y0 + 62.f, "WEAR: ADDS TO DUST. SHARE ON: MOVING IT HERE MOVES EVERY LINKED OND.", FAINT);
        pixelText(x0 + 14.f, y0 + 80.f, "PUMP ON: ANOTHER OND'S BEAT DUCKS THIS ONE (AS DEEP AS ITS OWN PUMP, BEAT VIEW).", FAINT);
        pixelText(x0 + 14.f, y0 + 98.f, "DRONE: THIS OND HOLDS THE ROOT (AND FIFTH) OF THE KEYS PLAYED IN THE OTHER ONDS.", FAINT);
        pixelText(x0 + 14.f, y0 + 116.f, "BRUDD ON: A BREAKER ON ANY LINKED OND BREAKS THIS ONE TOO, AND IT LANDS WITH THEM.", FAINT);
        static const char *const KEYS[6] = {"wear", "link_wear", "link_pump", "link_drone", "link_drone_oct", "link_brudd"};
        const float cw = (w - 28.f) / 6.f, cy = y0 + 236.f;
        for (int i = 0; i < 6; ++i) {
            const int p = ond::paramOfKey(KEYS[i]);
            if (p < 0)
                continue;
            const float cx = x0 + 14.f + cw * ((float)i + 0.5f);
            drawKnob(p, cx, cy, 30.f);
            hits.push_back({cx - cw / 2.f, cy - 70.f, cw, 140.f, p});
        }
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

    // the knob being turned, large, over the header: under a finger (a phone) the knob and its value are hidden
    void drawDragReadout()
    {
        if (drag < 0 || (size_t)drag >= ond::paramControls().size())
            return;
        ond_control_t info;
        controlInfo(drag, info);
        char buf[32];
        ond_control_text(ond::paramControls()[(size_t)drag], currentEngine(), intValue(drag), buf, sizeof buf);
        fill(120.f, 6.f, W - 240.f, 64.f, PANEL, 1.f, 8.f);
        pixelText(W / 2.f - 16.f, 22.f, info.label, DIM, 2.f, 1);
        pixelText(W / 2.f + 16.f, 22.f, buf, ACCENT, 2.f, -1);
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

    // ---- the sound browser: MINE (my presets), OND (the baseline eight), then every engine's sounds
    void drawBrowser()
    {
        fill(16.f, 84.f, 968.f, 600.f, PANEL, 1.f, 6.f);
        float y = 100.f;
        if (!browserLayer)                 // (the layer takes factory sounds)
            browserRow(y, "MINE", browserGroup == 1, -101);
        else
            pixelText(36.f, y, "FOR THE LAYER", ACCENT);
        y += 22.f;
        browserRow(y, "OND", browserGroup == 0, -100);
        y += 8.f;
        for (int e = 0; e < ond_engine_count(); ++e) {
            if (!ond_preset_count(e) || !std::strcmp(ond_engine_name(e), "SAMPLE"))
                continue;
            y += 22.f;
            browserRow(y, ond_engine_name(e), browserGroup == 2 && browserEngine == e, -200 - e);
        }
        fill(196.f, 96.f, 1.f, 576.f, EDGE);
        if (browserGroup == 1) {
            drawMine();
            return;
        }
        std::vector<int> list;
        const auto &all = ond::sounds();
        for (int i = 0; i < (int)all.size(); ++i)
            if (browserGroup == 0 ? i < ond::FAVOURITES : all[(size_t)i].engine == browserEngine)
                list.push_back(i);
        std::sort(list.begin(), list.end(), [&](int a, int b) {
            return browserGroup == 0 ? a < b : all[(size_t)a].preset < all[(size_t)b].preset;
        });
        for (size_t k = 0; k < list.size(); ++k) {
            const ond::Sound &s = all[(size_t)list[k]];
            const float bx = 216.f + 256.f * (float)(k / 22), byy = 100.f + 25.f * (float)(k % 22);
            const bool cur = browserLayer ? list[k] == layerIndex() : userName.empty() && list[k] == soundIndex();
            if (cur)
                fill(bx - 6.f, byy - 4.f, 244.f, 24.f, ACCENT, 0.16f, 3.f);
            pixelText(bx, byy, ond_preset_name(s.engine, s.preset), cur ? ACCENT : TEXT);
            hits.push_back({bx - 6.f, byy - 4.f, 244.f, 25.f, -1000 - list[k]});
        }
        pixelText(984.f - 14.f, 656.f, "ESC / CLICK THE NAME TO CLOSE", FAINT, 1.f, 1);
    }

    void drawMine()
    {
        if (mine.empty()) {
            pixelText(216.f, 100.f, "NO PRESETS OF YOUR OWN YET.", DIM);
            pixelText(216.f, 125.f, "SHAPE A SOUND, THEN SAVE AS (TOP RIGHT).", DIM);
        }
        for (size_t k = 0; k < mine.size() && k < 66u; ++k) {   // three columns of 22
            const float bx = 216.f + 256.f * (float)(k / 22), byy = 100.f + 25.f * (float)(k % 22);
            const bool cur = mine[k] == userName, armed = deleteArmed == (int)k;
            if (cur)
                fill(bx - 6.f, byy - 4.f, 244.f, 24.f, ACCENT, 0.16f, 3.f);
            pixelText(bx, byy, mine[k].c_str(), cur ? ACCENT : TEXT);
            hits.push_back({bx - 6.f, byy - 4.f, 200.f, 25.f, -2000 - (int)k});
            pixelText(bx + 230.f, byy, armed ? "DELETE?" : "x", armed ? ACCENT : FAINT, 1.f, 1);
            hits.push_back({bx + 160.f, byy - 4.f, 78.f, 25.f, -3000 - (int)k});
        }
        if (mineIndex() >= 0)
            button(216.f, 640.f, 90.f, 30.f, "RENAME", false, -30);
        button(mineIndex() >= 0 ? 316.f : 216.f, 640.f, 130.f, 30.f, "OPEN FOLDER", false, -31);
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
            const int armed = deleteArmed;
            deleteArmed = -1;
            if (h.param == -12) {
                browserOpen = false;
            } else if (h.param == -100) {
                browserGroup = 0;
            } else if (h.param == -101 && !browserLayer) {
                mine = ond::listPresets();
                browserGroup = 1;
            } else if (h.param <= -200 && h.param > -1000) {
                browserGroup = 2;
                browserEngine = -200 - h.param;
            } else if (h.param <= -1000 && h.param > -2000) {
                if (browserLayer) {
                    setValue(ond::layerSoundParam(), (float)(-1000 - h.param));
                    touch();
                } else {
                    selectSound(-1000 - h.param);
                }
                browserOpen = false;
            } else if (h.param <= -2000 && h.param > -3000) {
                loadUser(mine[(size_t)(-2000 - h.param)]);
                browserOpen = false;
            } else if (h.param <= -3000) {
                const int k = -3000 - h.param;
                if (armed != k) {
                    deleteArmed = k;               // once: asks
                } else {
                    if (mine[(size_t)k] == userName)
                        setUser("");               // (what sounds stays; it is just not a preset any more)
                    ond::deletePreset(mine[(size_t)k]);
                    mine = ond::listPresets();
                }
            } else if (h.param == -30) {
                startRename();
            } else if (h.param == -31) {
                ond::openPresetFolder();
            } else if (h.param == -10 || h.param == -11) {
                step(h.param == -11 ? 1 : -1);
            } else if (h.param == -20) {
                saveCurrent();
            } else if (h.param == -21) {
                saveAs();
            }
            repaint();
            return true;
        }
        deleteArmed = -1;
        return true;
    }

    std::vector<float> vals;
    std::vector<Hit> hits;
    int drag = -1, lastParam = -1;
    float dragY = 0.f, dragStart = 0.f;
    bool moved = false;
    uint lastClick = 0;
    bool browserOpen = false;
    int browserGroup = 0;                  // 0 OND (the eight), 1 MINE, 2 an engine (browserEngine)
    int browserEngine = 7;
    std::vector<std::string> mine;         // my presets' names
    std::string userName, editName;        // the preset loaded ("" a factory sound); the name being typed
    bool modified = false, renaming = false, fresh = false;
    int deleteArmed = -1;                  // a preset's delete asked once: the second click deletes
    int view = 0;                          // the lower half: 0 the sound's sections, 1 BRUDD's pads, 2 the LAYER
    bool browserLayer = false;             // the browser picks the layer's sound
    bool learning = false;                 // LEARN on: a click arms a breaker, the next pad hit is its
    int learnArmed = -1;
    ond::PadMap pads;                      // the pads learned (Documents/Ond/pads.txt, and the set's)
    int padDown = -1;                      // the pad held with the mouse
    int hitDown = -1;                      // the hit pad held with the mouse
    std::vector<std::string> hitNames;     // Documents/Ond/Hits: the first eight WAV files
    unsigned hitsReloads = 0;
    bool kitHas[16] = {};
    std::string savedMidi;                 // the last SAVE MIDI's file name

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OndUI)
};

UI *createUI() { return new OndUI(); }

END_NAMESPACE_DISTRHO
