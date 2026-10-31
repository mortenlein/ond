#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Ond as a Windows plugin (VST3, CLAP), cross-built with llvm-mingw. From the repo root:
#   sh tools/build_plugin.sh            -> build/plugin/bin/Ond.vst3, Ond.clap, Ond.exe (standalone)
# Windows (Git Bash): runs itself in WSL. Once, in WSL: llvm-mingw in ~/.llvm-mingw/current (or LLVM_MINGW),
# cmake, python3; git submodule update --init (third_party/DPF).
set -e
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Linux ] && command -v wsl.exe >/dev/null 2>&1; then
    exec env MSYS_NO_PATHCONV=1 wsl.exe -- sh tools/build_plugin.sh "$@"
fi
PY="${PYTHON:-python3}"
[ -f third_party/DPF/dgl/src/pugl-upstream/include/pugl/pugl.h ] || git submodule update --init --recursive third_party/DPF
mkdir -p build/gen
for g in tables:felucca_tables drumkits:felucca_drumkits fm6_patches:felucca_fm6 samples:felucca_samples; do
    [ "build/gen/${g#*:}.h" -nt "tools/gen_${g%%:*}.py" ] || "$PY" "tools/gen_${g%%:*}.py" "build/gen/${g#*:}.h" >/dev/null
done
"$PY" tools/gen_wavetables.py build/gen/felucca_wavetables.h >/dev/null   # (cached: only when the script changed)
# (the build tree on the Linux side: CMake on /mnt/c is slow and its file locks trip over Windows)
B="${OND_PLUGIN_BUILD:-$HOME/.cache/ond-plugin-build}"
cmake -S plugin -B "$B" -DCMAKE_TOOLCHAIN_FILE="$PWD/plugin/mingw-toolchain.cmake" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$B" -j"$(nproc)"
rm -rf build/plugin/bin
mkdir -p build/plugin/bin
cp -r "$B/bin/Ond.vst3" "$B/bin/Ond.clap" "$B/bin/Ond.exe" "$B/bin/ond-engine.dll" build/plugin/bin/
echo "build/plugin/bin: $(ls build/plugin/bin | tr '\n' ' ')"
