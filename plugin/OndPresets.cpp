// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// The player's own presets on disk (OndPresets.hpp). The only platform code: where Documents is, and opening the
// folder.
#include "OndPresets.hpp"
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
