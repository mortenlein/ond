// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// A log of the parameter changes Ond sees (0.26: in Live a click on the screen took two or three tries; the web and
// CLAP were fine): who set which parameter to what, when. Written from any thread into a ring (no locks, no files),
// written out by the screen (its idle) to %TEMP%\ond\ond-events.log, for Claude to read after Morten has clicked.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace ond {

struct EventLog {
    struct E {
        double ms;
        char who;                               // P the plugin's setParameterValue, U the screen sets, C the host
        int index;                              // tells the screen, X the screen ignored an echo, S a state
        float value;
    };
    E e[2048];
    std::atomic<uint32_t> w{0};
    uint32_t r = 0;

    static EventLog &get()
    {
        static EventLog log;
        return log;
    }
    static double now()
    {
        static const auto t0 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    void add(char who, int index, float value)
    {
        const uint32_t i = w.fetch_add(1);
        e[i % 2048] = {now(), who, index, value};
    }
    void flush()                                 // (the screen's thread)
    {
        const uint32_t end = w.load();
        if (end == r)
            return;
        const char *tmp = std::getenv("TEMP");
        if (!tmp) {
            r = end;
            return;
        }
        std::FILE *f = std::fopen((std::string(tmp) + "/ond/ond-events.log").c_str(), "a");
        for (; r != end; ++r)
            if (f && end - r <= 2048)
                std::fprintf(f, "%10.1f %c %3d %g\n", e[r % 2048].ms, e[r % 2048].who, e[r % 2048].index,
                             e[r % 2048].value);
        if (f)
            std::fclose(f);
    }
};

} // namespace ond
