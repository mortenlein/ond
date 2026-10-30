# Ond

A drone organ / harmonium with tape wear for the M-VAVE FM-1, desktop and phone.
A fork of [Clipper](https://github.com/isod89/clipper-fm1), built on
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments.
GPL-3.0-only.

Start with `AGENTS.md`, `docs/vision.md`, `docs/sound-lessons.md`.

## Setup (once)

```bash
# 1. Fork Clipper to your GitHub account as "ond" and clone it
mkdir -p ~/dev
cd ~/dev
gh repo fork isod89/clipper-fm1 --clone --fork-name ond
cd ~/dev/ond

# 2. Check the remotes: origin = mortenlein/ond, upstream = isod89/clipper-fm1
git remote -v

# 3. Unpack the scaffold into the repo (adjust the path to where the zip was downloaded)
unzip -o ~/Downloads/ond-scaffold.zip -d ~/dev/ond

# 4. Commit the scaffold
git add AGENTS.md docs/vision.md docs/sound-lessons.md docs/tasks README-ond.md
git commit -m "Ond: scaffold, vision and sound lessons"
git push

# 5. Start Claude Code in the repo
claude
```

Then tell Claude Code: `Read AGENTS.md and do docs/tasks/001-fork-and-baseline.md`.
