// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// One engine per Ond: the firmware keeps its state in globals (as on the FM-1), so every Ond in a Live set loads
// its own copy of the engine library (ond-engine.dll, next to the plugin's binary), copied to a file of its own
// (%TEMP%\ond\ond-engine-<pid>-<n>.dll): Windows gives each copy its own globals. The calls that change the engine
// go through here; the pure ones (names, ranges, value text, presets' values) stay on the plugin's own copy.
// Where the library cannot be loaded (another system, a missing file), the plugin's own copy plays, for one Ond.
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include "ond_engine.h"
}

namespace ond {

struct Engine {
    void (*init)(void) = ond_init;
    void (*load_preset)(int, int) = ond_load_preset;
    void (*load_layer)(int, int) = ond_load_layer;
    void (*midi)(uint8_t, uint8_t, uint8_t) = ond_midi;
    void (*all_notes_off)(void) = ond_all_notes_off;
    void (*set_tempo)(double) = ond_set_tempo;
    double (*tempo)(void) = ond_tempo;
    void (*transport)(int, double) = ond_transport;
    void (*play)(int) = ond_play;
    void (*drum)(int, int) = ond_drum;
    int (*beat_events)(uint8_t *, uint8_t *, int) = ond_beat_events;
    void (*pump_kick)(void) = ond_pump_kick;
    void (*control_set)(int, int) = ond_control_set;
    void (*breaker)(int) = ond_breaker;
    int (*breaker_playing)(void) = ond_breaker_playing;
    void (*hit)(int, int) = ond_hit;
    void (*hit_set)(int, const float *, const float *, uint32_t) = ond_hit_set;
    void (*kit_set)(int, const float *, const float *, uint32_t) = ond_kit_set;
    void (*render)(float *, float *, uint32_t) = ond_render;
    bool own = false;                           // true: a copy of its own (any number of Onds); false: the plugin's

#ifdef _WIN32
    HMODULE lib = nullptr;
    std::wstring copy;

    static std::wstring here()                  // the folder of the plugin's own binary
    {
        HMODULE self = nullptr;
        wchar_t p[MAX_PATH];
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&here, &self);
        const DWORD n = GetModuleFileNameW(self, p, MAX_PATH);
        std::wstring s(p, n);
        return s.substr(0, s.find_last_of(L"\\/") + 1);
    }

    template <typename F> bool get(F &f, const char *name)
    {
        f = reinterpret_cast<F>(reinterpret_cast<void *>(GetProcAddress(lib, name)));
        return f != nullptr;
    }

    bool load()
    {
        static std::atomic<unsigned> count{0};
        wchar_t tmp[MAX_PATH];
        const std::wstring src = here() + L"ond-engine.dll";
        if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES || !GetTempPathW(MAX_PATH, tmp))
            return false;
        const std::wstring dir = std::wstring(tmp) + L"ond\\";
        CreateDirectoryW(dir.c_str(), nullptr);
        {   // (copies an Ond that crashed left behind: whatever is not in use goes)
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((dir + L"ond-engine-*.dll").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do
                    DeleteFileW((dir + fd.cFileName).c_str());
                while (FindNextFileW(h, &fd));
                FindClose(h);
            }
        }
        copy = dir + L"ond-engine-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(count++) + L".dll";
        if (!CopyFileW(src.c_str(), copy.c_str(), FALSE) || !(lib = LoadLibraryW(copy.c_str()))) {
            DeleteFileW(copy.c_str());
            return false;
        }
        const bool ok = get(init, "ond_init") && get(load_preset, "ond_load_preset") && get(load_layer, "ond_load_layer") &&
                        get(midi, "ond_midi") && get(all_notes_off, "ond_all_notes_off") && get(set_tempo, "ond_set_tempo") &&
                        get(tempo, "ond_tempo") && get(transport, "ond_transport") && get(play, "ond_play") &&
                        get(drum, "ond_drum") && get(beat_events, "ond_beat_events") && get(pump_kick, "ond_pump_kick") && get(control_set, "ond_control_set") &&
                        get(breaker, "ond_breaker") && get(breaker_playing, "ond_breaker_playing") && get(hit, "ond_hit") &&
                        get(hit_set, "ond_hit_set") && get(kit_set, "ond_kit_set") && get(render, "ond_render");
        if (!ok) {
            unload();
            *this = Engine();                   // (the plugin's own, every call)
            return false;
        }
        own = true;
        return true;
    }

    void unload()
    {
        if (lib)
            FreeLibrary(lib);
        lib = nullptr;
        if (!copy.empty())
            DeleteFileW(copy.c_str());
        copy.clear();
        own = false;
    }
#else
    bool load() { return false; }
    void unload() {}
#endif
};

} // namespace ond
