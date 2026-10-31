#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
"""Ond's BEAT grooves (docs/tasks/015-lofi-beat.md) from real drummers: the Groove MIDI Dataset (Gillick, Roberts,
Engel, Tikhonov... Google Magenta, 2019; CC BY 4.0: magenta.tensorflow.org/datasets/groove).

  tools/gen_grooves.py groove-v1.0.0-midionly.zip host/ond_grooves.h

For each 4/4 beat (not a fill) at 65..100 BPM in the lo-fi styles (hip-hop, funk, soul, jazz): the 2 bars that
repeat best (a groove, not a fill: the most like the 2 bars after them) with a kick and a snare, on 32 sixteenths.
Each hit keeps its drummer's velocity (SLOOP's four levels, drums.c vel_lvl) and each step the drummer's timing
(SLOOP's per-step nudge, 1/64 of a step: the snare's if the step has one, else the kick's, else the loudest). The
kit's notes go to SLOOP's 16 lanes (drums.c LANE_NOTE). The output is committed: building Ond never needs the data.
"""
import csv
import io
import struct
import sys
import zipfile

LANES = {36: 0, 35: 1, 38: 2, 40: 2, 39: 3, 42: 4, 22: 4, 46: 5, 26: 5, 44: 6, 37: 7, 45: 9, 47: 9, 43: 9, 58: 9,
         48: 10, 50: 10, 49: 11, 55: 11, 57: 11, 52: 11, 51: 12, 59: 12, 53: 12}   # the TD-11's notes -> lanes
FEELS = [("HIP HOP", ("hiphop",)), ("FUNK", ("funk", "neworleans")), ("SOUL", ("soul",)), ("JAZZ", ("jazz",))]
LV_NORM, LV_GHOST, LV_SOFT, LV_HARD = 0, 1, 2, 3


def vel_lvl(v):                                   # drums.c vel_lvl: a MIDI velocity -> the level it records as
    return LV_GHOST if v < 56 else LV_SOFT if v < 88 else LV_NORM if v < 116 else LV_HARD


def midi_notes(data):
    """(ticks per beat, [(tick, note, velocity)]) of an SMF's note-ons, all tracks"""
    if data[:4] != b"MThd":
        raise ValueError("not MIDI")
    _, ntrk, tpb = struct.unpack(">HHH", data[8:14])
    pos, out = 14, []
    for _ in range(ntrk):
        if data[pos:pos + 4] != b"MTrk":
            break
        end = pos + 8 + struct.unpack(">I", data[pos + 4:pos + 8])[0]
        p, tick, run = pos + 8, 0, 0
        while p < end:
            dt = 0
            while True:
                b = data[p]; p += 1
                dt = dt << 7 | b & 0x7F
                if not b & 0x80:
                    break
            tick += dt
            st = data[p]
            if st == 0xFF:                        # meta
                ln, p2 = 0, p + 2
                while True:
                    b = data[p2]; p2 += 1
                    ln = ln << 7 | b & 0x7F
                    if not b & 0x80:
                        break
                p = p2 + ln
                continue
            if st in (0xF0, 0xF7):                # sysex
                ln, p2 = 0, p + 1
                while True:
                    b = data[p2]; p2 += 1
                    ln = ln << 7 | b & 0x7F
                    if not b & 0x80:
                        break
                p = p2 + ln
                continue
            if st & 0x80:
                run, p = st, p + 1
            kind, n = run & 0xF0, 1 if run & 0xF0 in (0xC0, 0xD0) else 2
            d = data[p:p + n]; p += n
            if kind == 0x90 and d[1] > 0:
                out.append((tick, d[0], d[1]))
        pos = end
    return tpb, out


def grid(notes, tpb):
    """per 16th (from tick 0): {lane: (offset in 1/64 of a step, velocity)}"""
    step = tpb / 4.0
    steps = {}
    for tick, note, vel in notes:
        lane = LANES.get(note)
        if lane is None:
            continue
        k = int(round(tick / step))
        off = int(round((tick - k * step) / step * 64))
        cell = steps.setdefault(k, {})
        if lane not in cell or vel > cell[lane][1]:
            cell[lane] = (max(-32, min(31, off)), vel)
    return steps


def window(steps, start):
    return {(k - start, lane) for k in range(start, start + 32) for lane in steps.get(k, {})}


def best_loop(steps, nsteps):
    best, score = None, 0.0
    for start in range(16, nsteps - 64 + 1, 16):              # (from bar 2: the first is often a count-in)
        a, b = window(steps, start), window(steps, start + 32)
        lanes = {lane for _, lane in a}
        if not a or 0 not in lanes or 2 not in lanes:
            continue
        s = len(a & b) / len(a | b)
        if s > score:
            best, score = start, s
    return best, score


def groove_steps(steps, start):
    out = []
    for k in range(start, start + 32):
        cell = steps.get(k, {})
        on = lvl = 0
        for lane, (_, vel) in cell.items():
            on |= 1 << lane
            lvl |= vel_lvl(vel) << (2 * lane)
        lead = 2 if 2 in cell else 0 if 0 in cell else max(cell, key=lambda l: cell[l][1]) if cell else None
        micro = cell[lead][0] if lead is not None else 0
        out.append((on, lvl, micro))
    return out


def main():
    src, dst = sys.argv[1], sys.argv[2]
    z = zipfile.ZipFile(src)
    info = next(n for n in z.namelist() if n.endswith("info.csv"))
    rows = list(csv.DictReader(io.TextIOWrapper(z.open(info), encoding="utf-8")))
    grooves = []
    for fi, (feel, styles) in enumerate(FEELS):
        for r in rows:
            if r["beat_type"] != "beat" or r["time_signature"] != "4-4" or not 65 <= int(r["bpm"]) <= 100:
                continue
            if r["style"].split("/")[0] not in styles:
                continue
            tpb, notes = midi_notes(z.read("groove/" + r["midi_filename"]))
            steps = grid(notes, tpb)
            if not steps:
                continue
            start, score = best_loop(steps, max(steps) + 1)
            if start is None or score < 0.5:
                continue
            grooves.append((fi, r["drummer"].replace("drummer", ""), int(r["bpm"]), r["style"], score,
                            groove_steps(steps, start)))
    grooves.sort(key=lambda g: (g[0], -g[4]))
    lines = ["/* generated by tools/gen_grooves.py from the Groove MIDI Dataset (Gillick, Roberts, Engel et al.,",
             " * Google Magenta, 2019; CC BY 4.0, magenta.tensorflow.org/datasets/groove): real drummers' two-bar",
             " * grooves, each step's hits (SLOOP's lanes), levels and nudge (1/64 of a step) as they played. Do not edit. */",
             "#pragma once", "#include <stdint.h>",
             "typedef struct { uint16_t on; uint32_t lvl; int8_t micro; } ond_gstep_t;",
             "typedef struct { uint8_t feel, drummer, bpm; const char *style; ond_gstep_t s[32]; } ond_groove_t;",
             "#define OND_NFEELS %d" % len(FEELS),
             "static const char *const OND_FEEL_NAMES[OND_NFEELS] = {%s};" % ", ".join('"%s"' % f for f, _ in FEELS),
             "static const ond_groove_t OND_GROOVES[] = {"]
    for fi, drummer, bpm, style, score, st in grooves:
        cells = ", ".join("{0x%04X, 0x%08X, %d}" % s for s in st)
        lines.append('    {%d, %s, %d, "%s", {%s}},   /* repeats %.0f%% */' % (fi, drummer, bpm, style, cells, 100 * score))
    lines.append("};")
    lines.append("#define OND_NGROOVES %d" % len(grooves))
    open(dst, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    for fi, (feel, _) in enumerate(FEELS):
        print("%-8s %d grooves" % (feel, sum(1 for g in grooves if g[0] == fi)))


if __name__ == "__main__":
    main()
