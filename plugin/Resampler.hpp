// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// Stereo resampler from the engine's 44.1 kHz to the host's rate: a windowed sinc (32 taps, Kaiser beta 9, cutoff
// 0.95 x the lower Nyquist), 512 phases interpolated linearly. At 44.1 kHz it is bypassed: the engine's samples as
// they are. Output n is the engine's sample n x 44100 / rate exactly (the engine renders up to HALF samples
// ahead to feed it): no latency to report; a MIDI event lands at most a block and HALF samples late (~1 ms).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

class Resampler {
public:
    static constexpr int HALF = 16, TAPS = 2 * HALF, PHASES = 512, RING = 1024;

    void setup(double inRate, double outRate)
    {
        step = inRate / outRate;
        bypass = std::fabs(step - 1.0) < 1e-9;
        const double fc = 0.95 * (step > 1.0 ? 1.0 / step : 1.0);   // of the input's Nyquist
        for (int p = 0; p <= PHASES; ++p)
            for (int k = 0; k < TAPS; ++k) {
                const double x = (k - HALF + 1) - (double)p / PHASES;   // tap k's distance from the read point
                const double s = x == 0.0 ? fc : std::sin(M_PI * fc * x) / (M_PI * x);
                const double w = x / HALF;
                table[p][k] = (float)(std::fabs(w) >= 1.0 ? 0.0 : s * bessel0(9.0 * std::sqrt(1.0 - w * w)) / bessel0(9.0));
            }
        reset();
    }

    void reset()
    {
        std::memset(ringL, 0, sizeof ringL);
        std::memset(ringR, 0, sizeof ringR);
        written = HALF - 1;                      // the first input sample sits at the first read point
        pos = 0.0;
    }

    bool isBypass() const { return bypass; }

    // input samples pushed beyond the next output's place (the engine's time ahead of the output's)
    double ahead() const { return bypass ? 0.0 : (double)(written - (HALF - 1)) - pos; }

    // input samples still to be pushed before the next output can be read
    bool needsInput() const { return (int64_t)pos + TAPS > written; }

    void push(float l, float r)
    {
        ringL[written % RING] = l;
        ringR[written % RING] = r;
        ++written;
    }

    void read(float &l, float &r)
    {
        const int64_t i0 = (int64_t)pos;
        const double frac = (pos - (double)i0) * PHASES;
        const int p = (int)frac;
        const float t = (float)(frac - p);
        float al = 0.f, ar = 0.f;
        for (int k = 0; k < TAPS; ++k) {
            const float c = table[p][k] + t * (table[p + 1][k] - table[p][k]);
            const int64_t j = (i0 + k) % RING;
            al += c * ringL[j];
            ar += c * ringR[j];
        }
        l = al;
        r = ar;
        pos += step;
    }

private:
    static double bessel0(double x)
    {
        double s = 1.0, t = 1.0;
        for (int k = 1; k < 40; ++k) {
            t *= (x / (2.0 * k)) * (x / (2.0 * k));
            s += t;
        }
        return s;
    }

    float table[PHASES + 1][TAPS];
    float ringL[RING], ringR[RING];
    int64_t written = 0;
    double pos = 0.0, step = 1.0;
    bool bypass = true;
};
