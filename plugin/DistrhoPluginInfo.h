// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morten Lein
#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Ond"
#define DISTRHO_PLUGIN_NAME    "Ond"
#define DISTRHO_PLUGIN_URI     "https://github.com/mortenlein/ond"
#define DISTRHO_PLUGIN_CLAP_ID "no.mortenlein.ond"

#define DISTRHO_PLUGIN_BRAND_ID  MLen
#define DISTRHO_PLUGIN_UNIQUE_ID OndS

#define DISTRHO_PLUGIN_HAS_UI           1
#define DISTRHO_UI_USE_NANOVG           1
#define DISTRHO_UI_USER_RESIZABLE       0
#define DISTRHO_UI_DEFAULT_WIDTH        1000
#define DISTRHO_UI_DEFAULT_HEIGHT       700
#define DISTRHO_PLUGIN_IS_RT_SAFE       1
#define DISTRHO_PLUGIN_IS_SYNTH         1
#define DISTRHO_PLUGIN_NUM_INPUTS       0
#define DISTRHO_PLUGIN_NUM_OUTPUTS      2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_TIMEPOS     1
#define DISTRHO_PLUGIN_VST3_CATEGORIES  "Instrument|Synth"
#define DISTRHO_PLUGIN_CLAP_FEATURES    "instrument", "synthesizer", "stereo"

#endif
