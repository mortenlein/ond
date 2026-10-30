# Task 002 — Build the FM-1 firmware, named Ond

**Goal:** `build/felucca.fwsc` builds from this repo, and the FM-1 (and the fm1-emu browser emulator)
shows Ond when it starts. No sound changes in this task.

## Steps

1. Toolchain in WSL Ubuntu (Linux x86-64, no Docker needed): `tools/get_toolchain.sh` (→ `~/.jieli/toolchain`)
   and the AC79 SDK `AC79NN_SDK_V1.2.1_2023-12-13` (→ `~/fw-AC79_AIoT_SDK`, see BUILDING.md), Pillow for
   `python3` in WSL.
2. Build Clipper unchanged first (`./build.sh` in WSL) and keep the result as the reference: same size,
   the build's own checks pass.
3. Name it Ond where a person sees it: the boot screen and the menu / console version.
   Constraints found in the code (do not break them):
   - The **package identity** (`FM-1_900`, `tools/build.py` PRODUCT, `felucca.c` / `loader.c` FELUCCA_ID)
     must stay `FM-1_9xx`: the installer checks the model ("FM-1") and the update loader answers as
     `ota-FM-1_9xx`. A different identity would be refused, and recovery depends on it. Keep it; Morten
     decides the release number scheme later.
   - The **version string sent to the editor** (`FELUCCA_VERSION`, `editor.c` `ed_str("FELUCCA " ...)`)
     is how `web/editor.html` knows the synth from the groovebox (`/CLIPPER/`) and which backup format it
     speaks (`bkDevFormat`, from Clipper's version number). Keep it as Clipper's until Ond has an editor of
     its own; show the Ond name separately.
   - The flash is nearly full (Clipper 1.2 had to compile the UI for size). Say what the change costs.
4. Build again; compare with step 2 (size, checks), and run the host tests that cover the boot screen.
5. Hand Morten `build/felucca.fwsc` to open in fm1-emu ("open a .fwsc"): does it start, show Ond, and
   sound like Clipper?
6. Commit: "firmware: builds as Ond (boot screen), sound unchanged".

## Done when
- One command in WSL builds the package from a clean checkout.
- Morten has opened it in the emulator and confirms it starts and sounds as before.

## Not in this task
- A new logo (Morten's call, later).
- Any DSP change. Installing on the real FM-1 is Morten's step, at his own risk.

## Status: ready to try in the emulator

- `sh tools/build_fw.sh` (Windows: runs in WSL) → `build/felucca.fwsc`, identity `FM-1_900`.
- Stock Clipper 1.3: image 562020 B, rebuilt byte for byte the same (sha256 cb976dff…).
- Ond: image 556980 B (5040 B less: the boot screen is the name in the screen font, Clipper's logo bitmap no
  longer linked; free in the app slot: 24584 B). No DSP file changed.
- Host tests (`SOAK_MIN=1 SKIP_CPU_VALGRIND=1 tests/run_tests.sh`): all pass except
  `tests/target_budget.py`, which fails the same way on stock Clipper 1.3 (its budget file is SLOOP's:
  `drums_mix` is gone from the FM-1 image, `fm1_alnk0_irq` over the old budget). Upstream, not Ond.
