// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// Ond as a plugin (DPF: VST3, CLAP): Clipper's DSP from the FM-1 sources (host/ond_engine.c) played by the host's
// MIDI, at 44.1 kHz, resampled to the host's rate. The firmware keeps its state in globals: one instance plays,
// a second one in the same process stays silent (gOwner) rather than corrupting the first.
#include "DistrhoPlugin.hpp"
#include "OndPresets.hpp"
#include "OndShared.hpp"
#include "Resampler.hpp"
#include <atomic>
#include <cmath>
#include <memory>

START_NAMESPACE_DISTRHO

namespace {
std::atomic<void *> gOwner{nullptr};
}

class OndPlugin : public Plugin {
public:
    OndPlugin()
        : Plugin((uint32_t)ond::paramCount(), 0, 2),
          values(new std::atomic<float>[(size_t)ond::paramCount()]),
          dirty(new std::atomic<bool>[(size_t)ond::paramCount()]),
          touched(new std::atomic<bool>[(size_t)ond::paramCount()])
    {
        for (int i = 0; i < ond::paramCount(); ++i) {
            Parameter p;
            initParameter((uint32_t)i, p);
            values[i] = p.ranges.def;
            dirty[i] = touched[i] = false;
        }
        setPads(ond::PadMap::parse(ond::loadPadsText().c_str()));   // the pads learned (every set's, until a
    }                                                                 // set brings its own)

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
    uint32_t getVersion() const override { return d_version(0, 3, 0); }

    // "preset": the player's preset loaded on the screen (its name; "" a factory sound), kept with the Live set
    // so the screen shows it again. The sound itself is in the parameters.
    // "pads": Brudd's pads learned on the screen (OndShared.hpp PadMap), kept with the set too
    void initState(uint32_t index, State &state) override
    {
        state.key = index == 0 ? "preset" : "pads";
        state.defaultValue = "";
        state.label = index == 0 ? "Preset" : "Pads";
        state.hints = 0;
    }
    String getState(const char *key) const override
    {
        return !std::strcmp(key, "preset") ? userPreset : !std::strcmp(key, "pads") ? String(padsText.c_str()) : String();
    }
    void setState(const char *key, const char *value) override
    {
        if (!std::strcmp(key, "preset"))
            userPreset = value;
        else if (!std::strcmp(key, "pads") && value[0])
            setPads(ond::PadMap::parse(value));
    }

    void initParameter(uint32_t index, Parameter &p) override
    {
        const int c = ond::paramControls()[index];
        if (c == -1) {
            p.hints = kParameterIsAutomatable | kParameterIsInteger;
            p.name = "Sound";
            p.symbol = "sound";
            p.ranges.min = 0.f;
            p.ranges.max = (float)(ond::sounds().size() - 1);
            p.ranges.def = 0.f;
            return;
        }
        if (c == -3 || c == -4) {               // Brudd: the breaker held (the screen's pads, automation)
            p.hints = c == -3 ? kParameterIsAutomatable | kParameterIsInteger : kParameterIsOutput | kParameterIsInteger;
            p.name = c == -3 ? "Breaker" : "Breaker Playing";
            p.symbol = c == -3 ? "breaker" : "breaker_out";
            p.ranges.min = 0.f, p.ranges.max = (float)ond_breaker_count(), p.ranges.def = 0.f;
            if (c == -3) {
                ParameterEnumerationValue *ev = new ParameterEnumerationValue[ond_breaker_count() + 1];
                ev[0].value = 0.f, ev[0].label = "OFF";
                for (int i = 0; i < ond_breaker_count(); ++i)
                    ev[i + 1].value = (float)(i + 1), ev[i + 1].label = ond_breaker_name(i);
                p.enumValues.count = (uint8_t)(ond_breaker_count() + 1);
                p.enumValues.restrictedMode = true;
                p.enumValues.values = ev;
            }
            return;
        }
        if (c == -5) {
            p.hints = kParameterIsOutput | kParameterIsInteger;
            p.name = "MIDI In";
            p.symbol = "midi_in";
            p.ranges.min = 0.f, p.ranges.max = 65535.f, p.ranges.def = 0.f;
            return;
        }
        if (c == -2) {
            p.hints = kParameterIsOutput;
            p.name = "Meter";
            p.symbol = "meter";
            p.ranges.min = 0.f, p.ranges.max = 1.f, p.ranges.def = 0.f;
            return;
        }
        ond_control_t info;
        ond_control_info(c, ond::sounds()[0].engine, &info);
        p.symbol = info.key;
        p.name = info.scope == OND_SCOPE_EDIT ? String("Edit ") + String(info.key + 4) : String(nameOf(info.key));
        if (info.scope == OND_SCOPE_EDIT) {            // 0..1 over the engine's range (OndShared.hpp)
            p.hints = kParameterIsAutomatable;
            p.ranges.min = 0.f, p.ranges.max = 1.f;
            p.ranges.def = ond::editNorm(ond_preset_value(c, ond::sounds()[0].engine, ond::sounds()[0].preset), info);
            return;
        }
        p.hints = kParameterIsAutomatable | kParameterIsInteger;
        p.ranges.min = (float)info.min;
        p.ranges.max = (float)info.max;
        // a fresh instance is the first sound as the FM-1 loads it; the player's own (kept) at their defaults,
        // reverb and delay dry
        p.ranges.def = (float)(info.kept ? (std::strcmp(info.key, "reverb") && std::strcmp(info.key, "delay") ? info.def : 0)
                                         : ond_preset_value(c, ond::sounds()[0].engine, ond::sounds()[0].preset));
        if (info.is_enum) {
            const int n = info.max - info.min + 1;
            ParameterEnumerationValue *ev = new ParameterEnumerationValue[n];
            for (int i = 0; i < n; ++i) {
                char buf[32];
                ond_control_text(c, ond::sounds()[0].engine, info.min + i, buf, sizeof buf);
                ev[i].value = (float)(info.min + i);
                ev[i].label = buf;
            }
            p.enumValues.count = (uint8_t)n;
            p.enumValues.restrictedMode = true;
            p.enumValues.values = ev;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        return index < (uint32_t)ond::paramCount() ? values[index].load() : 0.f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index >= (uint32_t)ond::paramCount() || (ond::paramControls()[index] <= -2 && ond::paramControls()[index] != -3))
            return;                             // (the outputs)
        values[index] = value;
        touched[index] = true;                  // set by the host or the screen: not the sound's own any more
        dirty[index] = true;                    // applied on the audio thread, at the next run
    }

    void activate() override
    {
        void *none = nullptr;
        owner = gOwner.compare_exchange_strong(none, this) || none == this;
        if (!owner)
            return;
        ond_init();                             // the sound, then only what was set over it (a restored set, the
        dirty[ond::P_SOUND] = true;             // player's own): an untouched knob keeps the sound's value
        for (int i = 1; i < ond::paramCount(); ++i)
            dirty[i] = touched[i].load();
        resampler.setup(OND_FS, getSampleRate());
        blockPos = OND_BLOCK;
        meter = 0.f;
        padsHeld = 0;
        breakerNow = -1;
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
        const TimePosition &tp = getTimePosition();
        if (tp.bbt.valid)
            ond_set_tempo(tp.bbt.beatsPerMinute);
        applyParameters();
        uint32_t done = 0, e = 0;
        while (done < frames) {
            for (; e < evCount && ev[e].frame <= done; ++e)
                if (ev[e].size <= 3u)
                    midiIn(ev[e].data[0], ev[e].size > 1u ? ev[e].data[1] : 0, ev[e].size > 2u ? ev[e].data[2] : 0);
            updateBreaker();
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
        float peak = 0.f;                       // the screen's meter: the peak, falling ~20 dB a second
        for (uint32_t i = 0; i < frames; ++i)
            peak = std::fmax(peak, std::fmax(std::fabs(outL[i]), std::fabs(outR[i])));
        meter = std::fmax(peak, meter * std::pow(0.1f, (float)frames / (float)getSampleRate()));
        values[ond::meterParam()] = meter;
        values[ond::breakerOutParam()] = (float)(ond_breaker_playing() + 1);
        values[ond::midiInParam()] = (float)midiSeen;
    }

private:
    static const char *nameOf(const char *key)
    {
        static const char *const N[][2] = {
            {"brite", "Brite"}, {"timbr", "Timbre"}, {"motn", "Motion"}, {"space", "Space"},
            {"attack", "Attack"}, {"decay", "Decay"}, {"sustain", "Sustain"}, {"release", "Release"},
            {"vintage", "Vintage"}, {"spread", "Spread"}, {"glide", "Glide"}, {"detune", "Detune"},
            {"voice", "Voice"}, {"lfo_rate", "LFO Rate"}, {"lfo_wave", "LFO Wave"}, {"lfo_pitch", "LFO > Pitch"},
            {"lfo_filter", "LFO > Filter"}, {"arp_mode", "Arp Mode"}, {"arp_rate", "Arp Rate"},
            {"arp_oct", "Arp Octaves"}, {"arp_gate", "Arp Gate"}, {"arp_hold", "Hold"}, {"arp_prob", "Arp Probability"},
            {"spice", "Spice"},
            {"dust", "Dust"}, {"filter", "Filter"}, {"chorus", "Chorus"}, {"chorus_rate", "Chorus Rate"},
            {"chorus_depth", "Chorus Depth"}, {"delay", "Delay"}, {"delay_time", "Delay Time"},
            {"delay_feedback", "Delay Feedback"}, {"delay_mode", "Delay Mode"}, {"reverb", "Reverb"},
            {"reverb_type", "Reverb Type"}, {"reverb_size", "Reverb Size"}, {"reverb_damp", "Reverb Damping"},
            {"level", "Level"}};
        for (const auto &n : N)
            if (!std::strcmp(n[0], key))
                return n[1];
        return key;
    }

    void setPads(const ond::PadMap &m)          // (not the audio thread: the code -> breaker table, atomically)
    {
        for (int c = 0; c < 256; ++c)
            padOf[c] = -1;
        for (int b = 0; b < ond::NBREAK; ++b)
            if (m.code[b] >= 0)
                padOf[m.code[b]] = (int8_t)b;
        padsLearned = !m.empty();
        padsText = m.text();
    }

    // a note or CC learned for a breaker holds it, on any channel (Live hands a plugin everything on channel 1);
    // until pads are learned, channel 10's bank B (notes 44..51) does, where a host keeps the channel, and the
    // rest of channel 10 never plays the organ (docs/controller-mpk-mini.md). The last pressed wins; let go, the
    // one held before it again
    void midiIn(uint8_t st, uint8_t d1, uint8_t d2)
    {
        const uint8_t kind = st & 0xF0u;
        const bool note = kind == 0x90u || kind == 0x80u, cc = kind == 0xB0u;
        const bool press = (kind == 0x90u && d2 > 0) || (cc && d2 >= 64u);
        int b = -1;
        if ((kind == 0x90u && d2 > 0) || (cc && d2 > 0))   // (the screen's LEARN and readout)
            midiSeen = ((midiSeen / 4096 + 1) % 16) * 4096 + (st & 0x0F) * 256 + (cc ? 128 : 0) + (d1 & 127);
        if (note || cc)
            b = padOf[(cc ? 128 : 0) + (d1 & 127)];
        if (b < 0 && !padsLearned && note && (st & 0x0Fu) == 9u) {
            const int pad = (int)d1 - ond::PAD_B_FIRST;
            if (pad < 0 || pad >= 8)
                return;                          // (channel 10: Brudd's, never the organ)
            b = ond::PAD_B[pad];
        }
        if (b < 0) {
            ond_midi(st, d1, d2);
            return;
        }
        for (int i = 0; i < padsHeld; ++i)       // (out of the stack wherever it is)
            if (padStack[i] == b) {
                for (int k = i; k + 1 < padsHeld; ++k)
                    padStack[k] = padStack[k + 1];
                --padsHeld;
                break;
            }
        if (press && padsHeld < ond::NBREAK)
            padStack[padsHeld++] = b;
    }

    void updateBreaker()                        // the pads first, else the parameter (the screen, automation)
    {
        const int param = (int)std::lround(values[ond::breakerParam()].load()) - 1;
        const int want = padsHeld ? padStack[padsHeld - 1] : param;
        if (want != breakerNow) {
            breakerNow = want;
            ond_breaker(want);
        }
    }

    void engineSample(float &l, float &r)
    {
        if (blockPos >= OND_BLOCK) {
            ond_render(blockL, blockR, OND_BLOCK);
            blockPos = 0;
        }
        l = blockL[blockPos];
        r = blockR[blockPos++];
    }

    // the parameters changed since the last run, on the audio thread. SOUND first: a preset loads whole (the
    // screen sends its values after it); then whatever the host or the screen set, and the player's own
    // (kept: sends, level) again over what the preset reset
    void applyParameters()
    {
        const int n = ond::paramCount();
        if (dirty[ond::P_SOUND].exchange(false)) {
            const auto &list = ond::sounds();
            const size_t i = (size_t)(int)values[ond::P_SOUND];
            current = list[i < list.size() ? i : 0];
            ond_load_preset(current.engine, current.preset);
            for (int p = 1; p < n; ++p) {
                ond_control_t info;
                const int c = ond::paramControls()[p];
                if (c < 0)
                    continue;
                ond_control_info(c, current.engine, &info);
                if (info.scope == OND_SCOPE_GLOBAL)
                    continue;
                if (info.kept)
                    dirty[p] = touched[p].load();   // the player's send / level over the preset's reset
                else if (!dirty[p])
                    touched[p] = false;             // the new sound's own value from now on
            }
        }
        for (int p = 1; p < n; ++p) {
            const int c = ond::paramControls()[p];
            if (c < 0 || !dirty[p].exchange(false))
                continue;
            ond_control_t info;
            ond_control_info(c, current.engine, &info);
            const float v = values[p];
            ond_control_set(c, info.scope == OND_SCOPE_EDIT ? ond::editValue(v, info) : (int)std::lround(v));
        }
    }

    void release()
    {
        void *self = this;
        if (owner)
            gOwner.compare_exchange_strong(self, nullptr);
        owner = false;
    }

    std::unique_ptr<std::atomic<float>[]> values;
    std::unique_ptr<std::atomic<bool>[]> dirty;
    std::unique_ptr<std::atomic<bool>[]> touched;
    ond::Sound current{7, 6};
    Resampler resampler;
    float blockL[OND_BLOCK], blockR[OND_BLOCK];
    uint32_t blockPos = OND_BLOCK;
    float meter = 0.f;
    int padStack[ond::NBREAK] = {}, padsHeld = 0, breakerNow = -1, midiSeen = 0;
    std::atomic<int8_t> padOf[256];
    std::atomic<bool> padsLearned{false};
    std::string padsText;
    bool owner = false;
    String userPreset;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OndPlugin)
};

Plugin *createPlugin() { return new OndPlugin(); }

END_NAMESPACE_DISTRHO
