// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// What the plugin's DSP and its screen share: the sound list and the parameters' order.
#pragma once
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "ond_engine.h"
}

namespace ond {

struct Sound {
    int engine, preset;
};

// every factory sound: the baseline eight (refs/baseline) first, then the engines closest to Ond (the order of
// 0.1, so its Live sets keep their sound), then the others. SAMPLE plays the user's samples: none here.
inline const std::vector<Sound> &sounds()
{
    static const std::vector<Sound> list = [] {
        static const Sound FIRST[] = {{7, 6}, {7, 13}, {7, 10}, {7, 12}, {12, 18}, {8, 6}, {22, 13}, {18, 5}};
        static const char *const NEAR[] = {"WHEEL", "STRINGS", "JUNO", "HYBRID", "GRAIN", "VECTOR",
                                           "TERRAIN", "WAVE", "MACRO", "TRIO", "ANALOG"};
        std::vector<Sound> v(FIRST, FIRST + sizeof FIRST / sizeof FIRST[0]);
        std::vector<int> order;
        for (const char *name : NEAR)
            for (int e = 0; e < ond_engine_count(); ++e)
                if (!std::strcmp(ond_engine_name(e), name))
                    order.push_back(e);
        for (int e = 0; e < ond_engine_count(); ++e) {
            bool seen = !std::strcmp(ond_engine_name(e), "SAMPLE");
            for (int o : order)
                seen |= o == e;
            if (!seen)
                order.push_back(e);
        }
        for (int e : order)
            for (int p = 0; p < ond_preset_count(e); ++p) {
                bool dup = false;
                for (const Sound &f : FIRST)
                    dup |= f.engine == e && f.preset == p;
                if (!dup)
                    v.push_back({e, p});
            }
        return v;
    }();
    return list;
}

inline constexpr int FAVOURITES = 8;          // sounds()[0..7]: the baseline eight

// plugin parameters: 0 SOUND, then the engine's controls (0.1's six first: their ids stay), then Brudd's breaker
// (held: 0 none, 1..16), what sounds of it (out), the last MIDI in (out), then the meter
inline const std::vector<int> &paramControls()  // parameter index -> control index (-1 SOUND, -2 meter,
                                                // -3 breaker, -4 breaker out, -5 MIDI in, -6 layer sound)
{
    static const std::vector<int> map = [] {
        static const char *const FIRST[] = {"dust", "reverb", "reverb_type", "reverb_size", "delay", "level"};
        std::vector<int> m{-1};
        auto find = [](const char *key) {
            ond_control_t c;
            for (int i = 0; i < ond_control_count(); ++i) {
                ond_control_info(i, 0, &c);
                if (!std::strcmp(c.key, key))
                    return i;
            }
            return -1;
        };
        for (const char *k : FIRST)
            m.push_back(find(k));
        // (ids stay: 0.4's Brudd parameters after 0.3's controls; anything newer after them, at the end)
        const int newer = find("layer_mode");
        for (int i = 0; i < ond_control_count(); ++i) {
            bool seen = false;
            for (const char *k : FIRST)
                seen |= find(k) == i;
            if (i == newer) {
                m.push_back(-3);
                m.push_back(-4);
                m.push_back(-5);
                m.push_back(-2);
            }
            if (i == find("catch"))
                m.push_back(-6);                // (0.5) the layer's sound, after 0.5's layer controls
            if (!seen)
                m.push_back(i);
        }
        return m;
    }();
    return map;
}

inline int paramCount() { return (int)paramControls().size(); }
inline int paramOfControl(int control)
{
    for (int i = 0; i < paramCount(); ++i)
        if (paramControls()[i] == control)
            return i;
    return -1;
}
inline int paramOfKey(const char *key)
{
    ond_control_t c;
    for (int i = 0; i < ond_control_count(); ++i) {
        ond_control_info(i, 0, &c);
        if (!std::strcmp(c.key, key))
            return paramOfControl(i);
    }
    return -1;
}
inline constexpr int P_SOUND = 0;
inline int paramOfCode(int code)            // -2 meter, -3 breaker, -4 breaker out, -5 MIDI in, -6 layer sound
{
    for (int i = paramCount() - 1; i >= 0; --i)
        if (paramControls()[(size_t)i] == code)
            return i;
    return -1;
}
inline int meterParam() { return paramOfCode(-2); }
inline int breakerParam() { return paramOfCode(-3); }
inline int breakerOutParam() { return paramOfCode(-4); }
// the last MIDI in (out, for the screen: LEARN, and what arrives): count % 16 * 4096 + channel * 256 + CC 128 + number
inline int midiInParam() { return paramOfCode(-5); }
inline int layerSoundParam() { return paramOfCode(-6); }
inline constexpr int LAYER_SOUND_DEF = 4;   // sounds()[4]: HYBRID DRONE, as ond_init's layer

// the MPK Mini's pads on channel 10 (docs/controller-mpk-mini.md): bank A (notes 36..43) the hits (Brudd stage 2),
// bank B (44..51) the breakers, in docs/rhythm-breaks.md's order (the punch-in effects nearest each role):
// stutter, tape stop, dropout (GATE), crush, half speed, reverse, freeze (a beat held: LOOP 4), echo
inline constexpr int PAD_B_FIRST = 44;
inline constexpr int PAD_B[8] = {4, 6, 13, 11, 7, 5, 0, 14};

// pads learned on the screen (LEARN in the BRUDD view): breaker -> the MIDI that plays it, on any channel (Live
// hands a plugin everything on channel 1): a note 0..127, a CC 128 + number, -1 none. As text, for the plugin's
// state and Documents/Ond/pads.txt: "4=n44 6=c20" (breaker=note / cc)
inline constexpr int NBREAK = 16;
struct PadMap {
    int code[NBREAK];
    PadMap() { clear(); }
    void clear()
    {
        for (int &c : code)
            c = -1;
    }
    bool empty() const
    {
        for (int c : code)
            if (c >= 0)
                return false;
        return true;
    }
    void assign(int breaker, int c)        // one MIDI, one breaker
    {
        for (int &k : code)
            if (k == c)
                k = -1;
        if (breaker >= 0 && breaker < NBREAK)
            code[breaker] = c;
    }
    std::string text() const
    {
        std::string t;
        for (int i = 0; i < NBREAK; ++i)
            if (code[i] >= 0)
                t += (t.empty() ? "" : " ") + std::to_string(i) + (code[i] < 128 ? "=n" : "=c") + std::to_string(code[i] & 127);
        return t;
    }
    static PadMap parse(const char *s)
    {
        PadMap m;
        int b, v, k;
        char kind;
        while (s && std::sscanf(s, " %d=%c%d%n", &b, &kind, &v, &k) == 3) {
            if (b >= 0 && b < NBREAK && v >= 0 && v < 128 && (kind == 'n' || kind == 'c'))
                m.assign(b, kind == 'n' ? v : 128 + v);
            s += k;
        }
        return m;
    }
};
inline std::string codeName(int c)         // "NOTE 44", "CC 20"
{
    return c < 0 ? std::string() : (c < 128 ? "NOTE " : "CC ") + std::to_string(c & 127);
}

// EDIT 1..8 change range with the engine: the host sees them as 0..1, mapped onto the engine's range
inline int editValue(float x, const ond_control_t &c)
{
    const float v = (float)c.min + x * (float)(c.max - c.min);
    return (int)(v < 0.f ? v - 0.5f : v + 0.5f);
}
inline float editNorm(int v, const ond_control_t &c)
{
    return c.max > c.min ? (float)(v - c.min) / (float)(c.max - c.min) : 0.f;
}

} // namespace ond
