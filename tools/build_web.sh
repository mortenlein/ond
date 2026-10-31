#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Ond in the browser (docs/tasks/012-web.md): the standalone built with Emscripten -> build/web/ (Ond.html, .js,
# .wasm): the same engine and screen, Web Audio, Web MIDI. From the repo root:
#   sh tools/build_web.sh
# Windows (Git Bash): runs itself in WSL. Once, in WSL: emsdk in ~/emsdk (EMSDK), activated; cmake, python3.
set -e
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Linux ] && command -v wsl.exe >/dev/null 2>&1; then
    exec env MSYS_NO_PATHCONV=1 WSLENV="OND_WEB_VERSION:${WSLENV:-}" wsl.exe -- sh tools/build_web.sh "$@"
fi
EMSDK="${EMSDK:-$HOME/emsdk}"
[ -f "$EMSDK/emsdk_env.sh" ] || { echo "build_web: no emsdk in $EMSDK (git clone emsdk, ./emsdk install latest, activate)"; exit 1; }
export PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"   # (emsdk_env.sh wants bash; its paths are these)
export EMSDK
PY="${PYTHON:-python3}"
[ -f third_party/DPF/dgl/src/pugl-upstream/include/pugl/pugl.h ] || git submodule update --init --recursive third_party/DPF
mkdir -p build/gen build/web
for g in tables:felucca_tables drumkits:felucca_drumkits fm6_patches:felucca_fm6 samples:felucca_samples; do
    [ "build/gen/${g#*:}.h" -nt "tools/gen_${g%%:*}.py" ] || "$PY" "tools/gen_${g%%:*}.py" "build/gen/${g#*:}.h" >/dev/null
done
"$PY" tools/gen_wavetables.py build/gen/felucca_wavetables.h >/dev/null
B="${OND_WEB_BUILD:-$HOME/.cache/ond-web-build}"
emcmake cmake -S plugin -B "$B" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$B" --target Ond-jack -j"$(nproc)"
cp "$B"/bin/Ond.html "$B"/bin/Ond.js "$B"/bin/Ond.wasm build/web/ 2>/dev/null || cp "$B"/bin/Ond* build/web/
# Ond's own part of the page (plugin/web/ond-keys.html: the FM-1's keys, the fit to the screen, the source link)
"$PY" - <<'PYEOF'
p = 'build/web/Ond.html'
s = open(p, encoding='utf-8').read()
s = s.replace('  </body>', open('plugin/web/ond-keys.html', encoding='utf-8').read() + '  </body>', 1)
open(p, 'w', encoding='utf-8').write(s)
PYEOF
# each version asks for its own Ond.js / Ond.wasm (the server caches them an hour; Ond.html is never cached)
V="${OND_WEB_VERSION:-0.$(grep -o 'd_version(0, [0-9]*' plugin/OndPlugin.cpp | grep -o '[0-9]*$')}"
sed -i "s|\"Ond.js\")|\"Ond.js?v=$V\")|" build/web/Ond.html
sed -i "s|locateFile(\"Ond.wasm\")|locateFile(\"Ond.wasm?v=$V\")|" build/web/Ond.js
# the release (docs/tasks/012-web.md: ashguard deploys the newest web-* release)
rm -f build/ond-web-*.zip*
"$PY" -c "import sys,zipfile; z=zipfile.ZipFile(sys.argv[1],'w',zipfile.ZIP_DEFLATED,compresslevel=9); [z.write('build/web/'+f,f) for f in ('Ond.html','Ond.js','Ond.wasm')]" "build/ond-web-$V.zip"
(cd build && sha256sum "ond-web-$V.zip" > "ond-web-$V.zip.sha256")
echo "build/web: $(ls build/web | tr '\n' ' ')  release: build/ond-web-$V.zip"
