#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Ond: the baseline renders (docs/tasks/001-fork-and-baseline.md). From the repo root:
#   sh tools/render_refs.sh [OUTDIR]          (default refs/baseline)
# Builds the generated tables (build/gen) and the host renderer (build/host/hostsim) as the FM-1 runs it
# (CLIPPER_SYNTH=1: tests/run_tests.sh builds the groovebox, CLIPPER_SYNTH=0, which has no VECTOR / TERRAIN), then
# renders every preset below with PRESET=1, a held 3-note chord (C4 E4 G4, keys up 3 s before the end), 12 s, at
# DUST 0, 40 and 90. The delay and reverb sends are 0, as the FM-1 loads a factory preset (ui.c apply_preset_to:
# "the factory sounds dry"); hostsim's PRESET=1 alone would add the preset's own sends.
# Windows without a C compiler: runs itself in WSL (needs cc and python3 there).
set -e
cd "$(dirname "$0")/.."
if ! command -v "${CC:-cc}" >/dev/null 2>&1 && command -v wsl.exe >/dev/null 2>&1; then
    exec env MSYS_NO_PATHCONV=1 wsl.exe -- sh tools/render_refs.sh "$@"
fi
OUT="${1:-refs/baseline}"
PY="${PYTHON:-python3}"
SECS=12
DUSTS="0 40 90"
P_DLY=35                        # track parameter ids (core.h enum: P_DIST 33, P_CHOR 34, P_DLY 35, P_REV 36)
P_REV=36

mkdir -p build/gen build/host "$OUT"
for g in tables:felucca_tables drumkits:felucca_drumkits fm6_patches:felucca_fm6 samples:felucca_samples; do
    [ "build/gen/${g#*:}.h" -nt "tools/gen_${g%%:*}.py" ] || "$PY" "tools/gen_${g%%:*}.py" "build/gen/${g#*:}.h" >/dev/null
done
"$PY" tools/gen_wavetables.py build/gen/felucca_wavetables.h >/dev/null   # (cached: only when the script changed)
${CC:-cc} -O2 -w -DCLIPPER_SYNTH=1 -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o build/host/hostsim tests/hostsim.c -lm -lpthread

# name : engine : preset (firmware/src/engines.c ENGINES[], the engine's preset table; CLIPPER_SYNTH=1)
PRESETS="
chapel:7:6
reed-organ:7:13
soft-flute:7:10
string-org:7:12
drone:12:18
dark-drone:8:6
space-drone:22:13
fold-drone:18:5
"
for p in $PRESETS; do
    name=${p%%:*}; rest=${p#*:}; eng=${rest%%:*}; pre=${rest#*:}
    for d in $DUSTS; do
        f="$OUT/${name}_dust$d.wav"
        PRESET=1 CHORD=3 HOLD=1 SECS=$SECS DUST=$d PSET="$P_DLY:0,$P_REV:0" \
            build/host/hostsim "$eng" "$pre" 0 "$f"
        echo "$f"
    done
done
