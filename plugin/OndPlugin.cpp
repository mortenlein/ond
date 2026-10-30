// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// Ond as a plugin (DPF: VST3, CLAP): Clipper's DSP from the FM-1 sources (host/ond_engine.c) played by the host's
// MIDI, at 44.1 kHz, resampled to the host's rate. The firmware keeps its state in globals: one instance plays,
// a second one in the same process stays silent (gOwner) rather than corrupting the first.
#include "DistrhoPlugin.hpp"
#include "Resampler.hpp"
#include <atomic>
#include <vector>

extern "C" {
#include "ond_engine.h"
}

START_NAMESPACE_DISTRHO

namespace {

enum { pSound, pDust, pReverb, pRevType, pRevSize, pDelay, pLevel, pCount };

struct Sound {
    int engine, preset;
};

// the SOUND menu (at most 255 entries, DPF's enumeration): the baseline eight (refs/baseline), then every preset of
// the engines closest to Ond. The others come with a screen of our own.
std::vector<Sound> soundList()
{
    static const Sound FIRST[] = {{7, 6}, {7, 13}, {7, 10}, {7, 12}, {12, 18}, {8, 6}, {22, 13}, {18, 5}};
    static const char *const ENGINES[] = {"WHEEL", "STRINGS", "JUNO", "HYBRID", "GRAIN", "VECTOR",
                                          "TERRAIN", "WAVE", "MACRO", "TRIO", "ANALOG"};
    std::vector<Sound> v(FIRST, FIRST + sizeof FIRST / sizeof FIRST[0]);
    for (const char *name : ENGINES)
        for (int e = 0; e < ond_engine_count(); ++e) {
            if (std::strcmp(ond_engine_name(e), name) != 0)
                continue;
            for (int p = 0; p < ond_preset_count(e); ++p) {
                bool dup = false;
                for (const Sound &f : FIRST)
                    dup |= f.engine == e && f.preset == p;
                if (!dup && v.size() < 255)
                    v.push_back({e, p});
            }
        }
    return v;
}

std::atomic<void *> gOwner{nullptr};

} // namespace

class OndPlugin : public Plugin {
public:
    OndPlugin()
        : Plugin(pCount, 0, 0), sounds(soundList())
    {
        for (int i = 0; i < pCount; ++i) {
            Parameter p;
            initParameter((uint32_t)i, p);
            values[i] = p.ranges.def;
            dirty[i] = true;
        }
    }

    ~OndPlugin() override { release(); }

protected:
    const char *getLabel() const override { return "Ond"; }
    const char *getDescription() const override
    {
        return "Drone organ / harmonium from the M-VAVE FM-1's Clipper firmware: the same DSP, played from the host.";
    }
    const char *getMaker() const override { return "Morten Lein"; }
    const char *getHomePage() const override { return "https://github.com/mortenlein/ond"; }
    const char *getLicense() const override { return "GPL-3.0-only"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initParameter(uint32_t index, Parameter &p) override
    {
        int mn = 0, mx = 127, df = 0;
        p.hints = kParameterIsAutomatable | kParameterIsInteger;
        switch (index) {
        case pSound: {
            p.name = "Sound";
            p.symbol = "sound";
            p.ranges.min = 0.f;
            p.ranges.max = (float)(sounds.size() - 1);
            p.ranges.def = 0.f;
            ParameterEnumerationValue *ev = new ParameterEnumerationValue[sounds.size()];
            for (size_t i = 0; i < sounds.size(); ++i) {
                ev[i].value = (float)i;
                ev[i].label = String(ond_engine_name(sounds[i].engine)) + ": " +
                              ond_preset_name(sounds[i].engine, sounds[i].preset);
            }
            p.enumValues.count = (uint8_t)sounds.size();
            p.enumValues.restrictedMode = true;
            p.enumValues.values = ev;
            return;
        }
        case pDust:
            p.name = "Dust", p.symbol = "dust";
            ond_global_range(OND_G_DUST, &mn, &mx, &df);
            break;
        case pReverb:                           // the FM-1 loads a factory sound dry: 0 until turned up
            p.name = "Reverb", p.symbol = "reverb";
            ond_param_range(OND_P_REV, &mn, &mx, &df);
            df = 0;
            break;
        case pRevType: {
            static const char *const T[] = {"ROOM", "PLATE", "HALL", "SHIMMER"};
            p.name = "Reverb Type", p.symbol = "revtype";
            ond_global_range(OND_G_RTYPE, &mn, &mx, &df);
            ParameterEnumerationValue *ev = new ParameterEnumerationValue[4];
            for (int i = 0; i < 4; ++i)
                ev[i].value = (float)i, ev[i].label = T[i];
            p.enumValues.count = 4;
            p.enumValues.restrictedMode = true;
            p.enumValues.values = ev;
            break;
        }
        case pRevSize:
            p.name = "Reverb Size", p.symbol = "revsize";
            ond_global_range(OND_G_RSIZE, &mn, &mx, &df);
            break;
        case pDelay:
            p.name = "Delay", p.symbol = "delay";
            ond_param_range(OND_P_DLY, &mn, &mx, &df);
            df = 0;
            break;
        case pLevel:
            p.name = "Level", p.symbol = "level";
            ond_param_range(OND_P_LEVEL, &mn, &mx, &df);
            break;
        }
        p.ranges.min = (float)mn;
        p.ranges.max = (float)mx;
        p.ranges.def = (float)df;
    }

    float getParameterValue(uint32_t index) const override { return index < pCount ? values[index].load() : 0.f; }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index >= pCount)
            return;
        values[index] = value;
        dirty[index] = true;                    // applied on the audio thread, at the next run
    }

    void activate() override
    {
        void *none = nullptr;
        owner = gOwner.compare_exchange_strong(none, this) || none == this;
        if (!owner)
            return;
        ond_init();
        for (int i = 0; i < pCount; ++i)
            dirty[i] = true;
        resampler.setup(OND_FS, getSampleRate());
        blockPos = OND_BLOCK;
    }

    void deactivate() override
    {
        if (owner)
            ond_all_notes_off();
        release();
    }

    void sampleRateChanged(double rate) override { resampler.setup(OND_FS, rate); }

    void run(const float **, float **outputs, uint32_t frames, const MidiEvent *ev, uint32_t evCount) override
    {
        float *outL = outputs[0], *outR = outputs[1];
        if (!owner) {
            std::memset(outL, 0, sizeof(float) * frames);
            std::memset(outR, 0, sizeof(float) * frames);
            return;
        }
        applyParameters();
        uint32_t done = 0, e = 0;
        while (done < frames) {
            for (; e < evCount && ev[e].frame <= done; ++e)
                if (ev[e].size <= 3u)
                    ond_midi(ev[e].data[0], ev[e].size > 1u ? ev[e].data[1] : 0, ev[e].size > 2u ? ev[e].data[2] : 0);
            const uint32_t next = e < evCount && ev[e].frame < frames ? ev[e].frame : frames;
            for (; done < next; ++done) {
                if (resampler.isBypass()) {
                    engineSample(outL[done], outR[done]);
                    continue;
                }
                while (resampler.needsInput()) {
                    float l, r;
                    engineSample(l, r);
                    resampler.push(l, r);
                }
                resampler.read(outL[done], outR[done]);
            }
        }
    }

private:
    void engineSample(float &l, float &r)
    {
        if (blockPos >= OND_BLOCK) {
            ond_render(blockL, blockR, OND_BLOCK);
            blockPos = 0;
        }
        l = blockL[blockPos];
        r = blockR[blockPos++];
    }

    void applyParameters()
    {
        const int v[pCount] = {(int)values[pSound], (int)values[pDust], (int)values[pReverb], (int)values[pRevType],
                               (int)values[pRevSize], (int)values[pDelay], (int)values[pLevel]};
        if (dirty[pSound].exchange(false)) {
            const Sound &s = sounds[(size_t)v[pSound] < sounds.size() ? (size_t)v[pSound] : 0];
            ond_load_preset(s.engine, s.preset);
            dirty[pReverb] = dirty[pDelay] = dirty[pLevel] = true;   // the preset set them: ours again
        }
        if (dirty[pDust].exchange(false))
            ond_set_global(OND_G_DUST, v[pDust]);
        if (dirty[pRevType].exchange(false))
            ond_set_global(OND_G_RTYPE, v[pRevType]);
        if (dirty[pRevSize].exchange(false))
            ond_set_global(OND_G_RSIZE, v[pRevSize]);
        if (dirty[pReverb].exchange(false))
            ond_set_param(OND_P_REV, v[pReverb]);
        if (dirty[pDelay].exchange(false))
            ond_set_param(OND_P_DLY, v[pDelay]);
        if (dirty[pLevel].exchange(false))
            ond_set_param(OND_P_LEVEL, v[pLevel]);
    }

    void release()
    {
        void *self = this;
        if (owner)
            gOwner.compare_exchange_strong(self, nullptr);
        owner = false;
    }

    const std::vector<Sound> sounds;
    std::atomic<float> values[pCount];
    std::atomic<bool> dirty[pCount];
    Resampler resampler;
    float blockL[OND_BLOCK], blockR[OND_BLOCK];
    uint32_t blockPos = OND_BLOCK;
    bool owner = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OndPlugin)
};

Plugin *createPlugin() { return new OndPlugin(); }

END_NAMESPACE_DISTRHO
