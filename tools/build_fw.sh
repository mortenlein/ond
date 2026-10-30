#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Ond: build the FM-1 package (build/felucca.fwsc) with ./build.sh. From the repo root:
#   sh tools/build_fw.sh [--release X.Y]
# Windows (Git Bash): runs itself in WSL, where the JieLi toolchain runs natively (Linux x86-64, no Docker).
# Once, in WSL (BUILDING.md): tools/get_toolchain.sh, the AC79 SDK in ~/fw-AC79_AIoT_SDK, pip3 install Pillow.
set -e
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Linux ] && command -v wsl.exe >/dev/null 2>&1; then
    exec env MSYS_NO_PATHCONV=1 wsl.exe -- sh tools/build_fw.sh "$@"
fi
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
[ -x "$TC/pi32v2/bin/clang" ] || { echo "build_fw: no JieLi toolchain in $TC: sh tools/get_toolchain.sh"; exit 1; }
[ -f "$SDK/cpu/wl82/tools/uboot.boot" ] || { echo "build_fw: no AC79 SDK in $SDK (BUILDING.md: git clone ... AC79NN_SDK_V1.2.1_2023-12-13)"; exit 1; }
exec ./build.sh "$@"
