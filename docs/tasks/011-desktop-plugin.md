# Task 011 — Ond as a plugin in Ableton (Windows VST3)

**Goal:** Clipper's sound, as the FM-1 plays it, as a VST3 instrument in Ableton Live 12 on Morten's PC.
No sound changes: the same DSP sources as the firmware.

## Steps

1. `host/ond_engine.c` / `.h`: the firmware's DSP (CLIPPER_SYNTH 1) behind a small C API: MIDI in (through the
   firmware's own USB-MIDI queue), a factory preset as the FM-1 loads it, DUST / reverb / delay, render at
   44.1 kHz. Second core off (the firmware's one-core path). `host/ond_engine_test.c` renders through it.
   **Done.**
2. Compiler: the firmware uses GNU C (inline asm, `offsetof` in static asserts), which MSVC does not take. Build
   with clang for Windows: llvm-mingw (portable, in WSL, no install on Windows). **Needs Morten's OK to
   download** (github.com/mstorsjo/llvm-mingw, release 20261020).
3. Plugin framework: DPF (github.com/DISTRHO/DPF, ISC licence, GPL-compatible; writes VST3 itself, no
   Steinberg SDK) as a git submodule in `third_party/DPF`. **Needs Morten's OK to add.**
4. `plugin/`: the DPF plugin. MIDI in at its sample positions (to the engine's 32-frame blocks, 0.7 ms),
   a windowed-sinc resampler 44.1 kHz → the host's rate, parameters: SOUND (every factory preset, the
   baseline eight first), DUST, REVERB, REVERB TYPE, REVERB SIZE, DELAY, LEVEL. No custom screen yet: Ableton
   shows the parameters.
5. One instance per Live set for now (the firmware keeps its state in globals): a second instance stays
   silent rather than corrupting the first. Lifting that is its own task.
6. Build in WSL → `build/plugin/Ond.vst3`, copy to `C:\Program Files\Common Files\VST3` (Morten's step, or
   with his OK). Renders through the plugin path next to `refs/baseline/` for Morten to compare.

## Done when
- Morten plays Ond in Ableton and it sounds like the baseline renders / the emulator.
