// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// The player's own presets on disk (OndPresets.hpp). The only platform code: where Documents is, and opening the
// folder.
#include "OndPresets.hpp"
#include "Resampler.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace ond {

namespace {

const char *const EXT = ".ondpreset";

fs::path presetDir()
{
    if (const char *own = std::getenv("OND_PRESET_DIR")) {   // (tests: a folder of their own)
        std::error_code ec;
        fs::create_directories(fs::u8path(own), ec);
        return fs::u8path(own);
    }
    fs::path docs;
#ifdef _WIN32
    PWSTR p = nullptr;                     // Documents where Windows keeps it (moved, OneDrive...)
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p)))
        docs = fs::path(p);
    CoTaskMemFree(p);
#else
    if (const char *home = std::getenv("HOME"))
        docs = fs::path(home) / "Documents";
#endif
    if (docs.empty())
        docs = fs::temp_directory_path();
    fs::path d = docs / "Ond" / "Presets";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

std::string fileStem(const std::string &name)   // a name as a file name: no characters Windows refuses
{
    std::string s;
    for (char c : name)
        s += std::strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32 ? '-' : c;
    while (!s.empty() && (s.back() == '.' || s.back() == ' '))
        s.pop_back();
    return s.empty() ? "preset" : s;
}

fs::path pathOf(const std::string &name) { return presetDir() / fs::u8path(fileStem(name) + EXT); }

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool readFile(const fs::path &path, UserPreset &out)
{
    std::ifstream f(path);
    std::string line;
    if (!f)
        return false;
    out = UserPreset();
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos)
            continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "name")
            out.name = v;
        else if (k == "engine")
            out.engine = v;
        else if (k == "preset")
            out.preset = v;
        else if (k == "layer_engine")
            out.layerEngine = v;
        else if (k == "layer_preset")
            out.layerPreset = v;
        else if (k != "version")
            out.values[k] = std::atoi(v.c_str());
    }
    if (out.name.empty())
        out.name = path.stem().u8string();
    return true;
}

} // namespace

std::vector<std::string> listPresets()
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(presetDir(), ec)) {
        UserPreset p;
        if (e.path().extension() == EXT && readFile(e.path(), p))
            names.push_back(p.name);
    }
    std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b) { return lower(a) < lower(b); });
    return names;
}

bool loadPreset(const std::string &name, UserPreset &out) { return readFile(pathOf(name), out); }

bool savePreset(const UserPreset &p)
{
    const fs::path path = pathOf(p.name), tmp = fs::path(path).concat(".tmp");
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f)
            return false;
        f << "# Ond preset (github.com/mortenlein/ond)\nversion=1\nname=" << p.name << "\nengine=" << p.engine
          << "\npreset=" << p.preset << "\n";
        if (!p.layerEngine.empty())
            f << "layer_engine=" << p.layerEngine << "\nlayer_preset=" << p.layerPreset << "\n";
        for (const auto &kv : p.values)
            f << kv.first << "=" << kv.second << "\n";
        if (!f)
            return false;
    }
    std::error_code ec;                    // written whole, then put in place: a crash never leaves half a file
    fs::rename(tmp, path, ec);
    return !ec;
}

bool deletePreset(const std::string &name)
{
    std::error_code ec;
    return fs::remove(pathOf(name), ec);
}

bool presetExists(const std::string &name)
{
    std::error_code ec;
    return fs::exists(pathOf(name), ec);
}

bool renamePreset(const std::string &from, const std::string &to)
{
    UserPreset p;
    if (from == to || to.empty() || !loadPreset(from, p))
        return false;
    if (lower(fileStem(from)) != lower(fileStem(to)) && presetExists(to))
        return false;                      // never over another preset
    p.name = to;
    if (!savePreset(p))
        return false;
    if (lower(fileStem(from)) != lower(fileStem(to)))
        deletePreset(from);
    return true;
}

std::string freeName(const std::string &base)
{
    for (int i = 2; i < 1000; ++i) {
        const std::string n = base + " " + std::to_string(i);
        if (!presetExists(n))
            return n;
    }
    return base;
}

fs::path hitsDir()
{
    fs::path d = presetDir().parent_path() / "Hits";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

std::vector<std::string> listHits()
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(hitsDir(), ec))
        if (lower(e.path().extension().u8string()) == ".wav")
            names.push_back(e.path().stem().u8string());
    std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b) { return lower(a) < lower(b); });
    if (names.size() > 8)
        names.resize(8);
    return names;
}

bool readWav(const fs::path &path, std::vector<float> &l, std::vector<float> &r);
bool loadHit(const std::string &name, std::vector<float> &l, std::vector<float> &r)
{
    return readWav(hitsDir() / fs::u8path(name + ".wav"), l, r);
}

// RIFF WAVE: PCM 16 / 24 / 32-bit or float 32 (WAVE_FORMAT_EXTENSIBLE too), the first two channels
bool readWav(const fs::path &path, std::vector<float> &l, std::vector<float> &r)
{
    std::ifstream f(path, std::ios::binary);
    std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto u16 = [&](size_t i) { return (unsigned)d[i] | (unsigned)d[i + 1] << 8; };
    auto u32 = [&](size_t i) { return u16(i) | (unsigned)u16(i + 2) << 16; };
    unsigned fmt = 0, ch = 0, rate = 0, bits = 0;
    size_t data = 0, dlen = 0, i = 12;
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4))
        return false;
    while (i + 8 <= d.size()) {
        const size_t len = u32(i + 4), body = i + 8;
        if (!std::memcmp(d.data() + i, "fmt ", 4) && body + 16 <= d.size()) {
            fmt = u16(body), ch = u16(body + 2), rate = u32(body + 4), bits = u16(body + 14);
            if (fmt == 0xFFFE && body + 26 <= d.size())
                fmt = u16(body + 24);          // (the subformat's first word: 1 PCM, 3 float)
        } else if (!std::memcmp(d.data() + i, "data", 4)) {
            data = body, dlen = std::min(len, d.size() - body);
            break;
        }
        i = body + len + (len & 1u);
    }
    if (!data || !ch || !rate || !(fmt == 1 && (bits == 16 || bits == 24 || bits == 32)) && !(fmt == 3 && bits == 32))
        return false;
    const size_t bps = bits / 8, frames = std::min(dlen / (bps * ch), (size_t)rate * 20u);
    auto sample = [&](size_t fr, unsigned c) -> float {
        const size_t p = data + (fr * ch + c) * bps;
        if (fmt == 3) {
            float v;
            const unsigned w = u32(p);
            std::memcpy(&v, &w, 4);
            return v;
        }
        if (bits == 16)
            return (float)(int16_t)u16(p) / 32768.f;
        if (bits == 24)
            return (float)((int32_t)((unsigned)d[p] << 8 | (unsigned)d[p + 1] << 16 | (unsigned)d[p + 2] << 24) >> 8) / 8388608.f;
        return (float)(int32_t)u32(p) / 2147483648.f;
    };
    l.clear(), r.clear();
    if (rate == 44100) {
        for (size_t fr = 0; fr < frames; ++fr)
            l.push_back(sample(fr, 0)), r.push_back(sample(fr, ch > 1 ? 1 : 0));
        return true;
    }
    Resampler rs;                              // (the plugin's own: windowed sinc, 32 taps)
    rs.setup((double)rate, 44100.0);
    size_t fr = 0;
    const size_t out = (size_t)((double)frames * 44100.0 / rate);
    while (l.size() < out) {
        float a, b;
        while (rs.needsInput())
            fr < frames ? rs.push(sample(fr, 0), sample(fr, ch > 1 ? 1 : 0)) : rs.push(0.f, 0.f), ++fr;
        rs.read(a, b);
        l.push_back(a), r.push_back(b);
    }
    return true;
}

const char *const KIT_FILES[16] = {"kick", "kick2", "snare", "clap", "hat", "open", "pedal", "rim",
                                   "snare2", "tomlo", "tomhi", "crash", "ride", "shaker", "conga", "cowbell"};

fs::path kitDir()
{
    fs::path d = presetDir().parent_path() / "Kit";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

bool loadKitLane(int lane, std::vector<float> &l, std::vector<float> &r)
{
    std::error_code ec;
    if (lane < 0 || lane > 15)
        return false;
    for (const auto &e : fs::directory_iterator(kitDir(), ec))   // (any case: Kick.WAV too)
        if (lower(e.path().extension().u8string()) == ".wav" && lower(e.path().stem().u8string()) == KIT_FILES[lane])
            return readWav(e.path(), l, r);
    return false;
}

void openKitFolder()
{
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", kitDir().wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

fs::path beatsDir()
{
    fs::path d = presetDir().parent_path() / "Beats";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

std::string saveBeatMidi(const std::string &name, int bars, const uint8_t (*vel)[16][16], const int8_t (*micro)[16],
                         double bpm)
{
    const int Q = 480, STEP = Q / 4, LEN = Q / 8;   // a 16th 120 ticks; each note a 32nd long
    struct Ev { int t; uint8_t st, a, b; };
    std::vector<Ev> ev;
    for (int bar = 0; bar < bars; ++bar)
        for (int s = 0; s < 16; ++s) {
            const int t = (bar * 16 + s) * STEP + (micro[bar][s] * STEP + (micro[bar][s] >= 0 ? 32 : -32)) / 64;
            for (int l = 0; l < 16; ++l)
                if (vel[bar][s][l]) {
                    ev.push_back({t < 0 ? 0 : t, 0x90, (uint8_t)(16 + l), vel[bar][s][l]});
                    ev.push_back({(t < 0 ? 0 : t) + LEN, 0x80, (uint8_t)(16 + l), 0});
                }
        }
    std::stable_sort(ev.begin(), ev.end(), [](const Ev &x, const Ev &y) { return x.t < y.t || (x.t == y.t && x.st < y.st); });
    std::vector<uint8_t> trk;
    auto vlq = [&](uint32_t v) {
        uint8_t b[5];
        int n = 0;
        b[n++] = v & 0x7F;
        while (v >>= 7)
            b[n++] = (uint8_t)(0x80 | (v & 0x7F));
        while (n)
            trk.push_back(b[--n]);
    };
    const uint32_t us = (uint32_t)(60000000.0 / (bpm > 0 ? bpm : 90.0));
    vlq(0), trk.insert(trk.end(), {0xFF, 0x51, 0x03, (uint8_t)(us >> 16), (uint8_t)(us >> 8), (uint8_t)us});
    vlq(0), trk.insert(trk.end(), {0xFF, 0x58, 0x04, 4, 2, 24, 8});   // 4/4
    int last = 0;
    for (const Ev &e : ev) {
        vlq((uint32_t)(e.t - last));
        last = e.t;
        trk.push_back(e.st), trk.push_back(e.a), trk.push_back(e.b);
    }
    vlq((uint32_t)(bars * 16 * STEP - last > 0 ? bars * 16 * STEP - last : 0));   // (the clip's length: whole bars)
    trk.insert(trk.end(), {0xFF, 0x2F, 0x00});
    std::string free = fileStem(name);
    for (int i = 2; i < 1000; ++i) {
        std::error_code ec;
        if (!fs::exists(beatsDir() / fs::u8path(free + ".mid"), ec))
            break;
        free = fileStem(name) + " " + std::to_string(i);
    }
    std::ofstream f(beatsDir() / fs::u8path(free + ".mid"), std::ios::binary | std::ios::trunc);
    const uint8_t hdr[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (uint8_t)(Q >> 8), (uint8_t)Q,
                           'M', 'T', 'r', 'k', (uint8_t)(trk.size() >> 24), (uint8_t)(trk.size() >> 16),
                           (uint8_t)(trk.size() >> 8), (uint8_t)trk.size()};
    f.write((const char *)hdr, sizeof hdr);
    f.write((const char *)trk.data(), (std::streamsize)trk.size());
    return f ? free : std::string();
}

void openBeatsFolder()
{
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", beatsDir().wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

void openHitsFolder()
{
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", hitsDir().wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::system(("open \"" + hitsDir().u8string() + "\"").c_str());
#else
    std::system(("xdg-open \"" + hitsDir().u8string() + "\" &").c_str());
#endif
}

std::string loadPadsText()
{
    std::ifstream f(presetDir().parent_path() / "pads.txt");
    std::string line, all;
    while (std::getline(f, line))
        if (!line.empty() && line[0] != '#')
            all += line + " ";
    return all;
}

void savePadsText(const std::string &text)
{
    std::ofstream f(presetDir().parent_path() / "pads.txt", std::ios::trunc);
    f << "# Ond: Brudd's pads, learned on the screen (breaker=n<note> or c<cc>)\n" << text << "\n";
}

void openPresetFolder()
{
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", presetDir().wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::system(("open \"" + presetDir().u8string() + "\"").c_str());
#else
    std::system(("xdg-open \"" + presetDir().u8string() + "\" &").c_str());
#endif
}

} // namespace ond
