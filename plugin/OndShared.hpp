// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// What the plugin's DSP and its screen share: the sound list and the parameters' order.
#pragma once
#include <cstring>
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

// plugin parameters: 0 SOUND, then the engine's controls (0.1's six first: their ids stay), then the meter
inline const std::vector<int> &paramControls()  // parameter index -> control index (-1: SOUND, -2: meter)
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
        for (int i = 0; i < ond_control_count(); ++i) {
            bool seen = false;
            for (const char *k : FIRST)
                seen |= find(k) == i;
            if (!seen)
                m.push_back(i);
        }
        m.push_back(-2);
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
inline int meterParam() { return paramCount() - 1; }

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
