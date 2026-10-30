// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
// The player's own presets: one small text file each in Documents/Ond/Presets (*.ondpreset): the sound by its
// engine's and preset's names, then every control by its stable key (host/ond_engine.c) and the firmware's
// value. A key missing from a file (an older preset) keeps the sound's own value; unknown keys are skipped.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace ond {

struct UserPreset {
    std::string name;                      // as the player named it (the file's name follows it)
    std::string engine, preset;            // the factory sound under it
    std::map<std::string, int> values;     // control key -> the firmware's value
};

std::vector<std::string> listPresets();    // the names, sorted (case does not count)
bool loadPreset(const std::string &name, UserPreset &out);
bool savePreset(const UserPreset &p);      // writes (or overwrites) the file of p.name
bool deletePreset(const std::string &name);
bool renamePreset(const std::string &from, const std::string &to);
bool presetExists(const std::string &name);
std::string freeName(const std::string &base);   // "CHAPEL 2", "CHAPEL 3" ... the first not taken
void openPresetFolder();                   // in Explorer / Finder / the file manager

} // namespace ond
