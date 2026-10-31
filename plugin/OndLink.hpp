// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// LINK: the Onds of one Live set hear each other. The plugin's library is loaded once per process, so this hub (its
// statics) is shared by every Ond in the set. Each Ond has a slot: the notes played into it, its BEAT's settings
// (another Ond works out the kicks from them and Live's position: the pump sample-exact, no message late), its
// breaker; and one WEAR for all. Lock-free: atomics only, read and written from any Ond's audio thread.
#pragma once
#include <atomic>
#include <cstdint>

namespace ond {

struct LinkHub {
    static constexpr int SLOTS = 16;
    struct Slot {
        std::atomic<uint32_t> owner{0};         // 0 free, else the Ond's id
        std::atomic<uint32_t> notes[4];         // the keys held in it (128 bits; Ond's own low notes left out)
        std::atomic<int32_t> beat[12];          // BEAT's values as ond_beat_bar takes them (FEEL 0: no beat)
        std::atomic<int32_t> beatOn{0};         // its BEAT plays with Live (FEEL on, Live playing)
        std::atomic<int32_t> breaker{-1};       // the breaker its own pads / screen hold (-1 none)
        std::atomic<uint32_t> breakerSeq{0};    // (the latest pressed of all wins)
    };
    Slot slot[SLOTS];
    std::atomic<int32_t> wear{0};               // WEAR, shared (link_wear on)
    std::atomic<uint32_t> wearSeq{0};
    std::atomic<uint32_t> seq{1};
    std::atomic<uint32_t> nextId{1};

    static LinkHub &get()
    {
        static LinkHub hub;
        return hub;
    }
    int claim(uint32_t id)
    {
        for (int i = 0; i < SLOTS; ++i) {
            uint32_t z = 0;
            if (slot[i].owner.compare_exchange_strong(z, id)) {
                for (auto &n : slot[i].notes)
                    n = 0;
                slot[i].beatOn = 0;
                slot[i].breaker = -1;
                return i;
            }
        }
        return -1;                               // (more than 16 Onds: this one plays, unlinked)
    }
    void release(int i)
    {
        if (i < 0)
            return;
        slot[i].beatOn = 0;
        slot[i].breaker = -1;
        for (auto &n : slot[i].notes)
            n = 0;
        slot[i].owner = 0;
    }
    int count() const
    {
        int n = 0;
        for (const auto &s : slot)
            n += s.owner.load() != 0;
        return n;
    }
};

} // namespace ond
