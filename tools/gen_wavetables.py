#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Reproduce the original/CC0 WT Lab tables. No downloads or third-party Python packages.

    python tools/gen_wavetables.py
    python tools/gen_wavetables.py --check
"""
import argparse
import cmath
import hashlib
import json
import math
from pathlib import Path
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / "assets" / "wavetables" / "akwf"
OUTPUT = ROOT / "firmware" / "src" / "wt_data.h"
SIZE, FRAMES, PITCH_MAX = 256, 8, 134
CAPS = (127, 63, 31, 15, 7, 3, 1)
PEAK = 30000
INFO = (
    ("SINE-SAW", "A sine opens into all harmonics."),
    ("ODD", "A sine opens into a square's odd harmonics."),
    ("PULSE", "A centered pulse narrows from 50 to 10 percent."),
    ("FORMANT", "Two harmonic resonances move through the spectrum."),
    ("ORGAN", "Upper drawbars join the fundamental."),
    ("GLASS", "A narrow cluster of upper harmonics moves upward."),
    ("HOLLOW", "Moving comb notches reshape a harmonic spectrum."),
    ("AKWF VOX", "Eight phase-aligned AKWF vocal waves, ordered by brightness."),
    ("STRINGS", "Bowed cello cycles open from rounded to raspy."),
    ("REED", "Clarinet-like cycles move from mellow to woody."),
    ("E-PIANO", "Electric-piano cycles move from rounded to bright."),
    ("PLUCK", "Acoustic-guitar cycles move from warm to wiry."),
    ("CHIP", "Chip-oscillator cycles move from simple to complex."),
    ("GRIT", "Distorted cycles move from rounded to abrasive."),
)
AKWF_FAMILIES = ("strings", "reed", "epiano", "pluck", "chip", "grit")


def original(table, position):
    coeff = [0j] * (CAPS[0] + 1)
    for h in range(1, len(coeff)):
        if table == 0:
            a = 1.0 if h == 1 else position / h
        elif table == 1:
            a = (1.0 if h == 1 else position / h) if h & 1 else 0.0
        elif table == 2:
            duty = 0.5 - 0.4 * position
            coeff[h] = math.sin(math.pi * h * duty) / h * cmath.exp(-0.5j * math.pi * h)
            continue
        elif table == 3:
            center = 2.0 + 14.0 * position
            a = 0.7 if h == 1 else (
                math.exp(-((h - center) / 1.6) ** 2) +
                0.5 * math.exp(-((h - center * 2.2) / 2.8) ** 2)) / math.sqrt(h)
        elif table == 4:
            a = 1.0 if h == 1 else (
                position ** (1.0 + h / 20.0) / math.sqrt(h)
                if h in (2, 3, 4, 6, 8, 12, 16) else 0.0)
        elif table == 5:
            a = 0.6 if h == 1 else math.exp(-((h - 3.0 - 28.0 * position) / 2.0) ** 2)
        elif table == 6:
            a = 1.0 if h == 1 else (
                0.1 + 0.9 * math.sin(h * (0.25 + 0.7 * position)) ** 2) / h
        else:
            raise ValueError(f"unknown original table: {table}")
        coeff[h] = -0.5j * a
    return coeff


def verified_bytes(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"{path}: source checksum mismatch")
    return data


def akwf_frames(directory=SOURCES):
    manifest = json.loads((directory / "manifest.json").read_text(encoding="ascii"))
    verified_bytes(SOURCES / "LICENSE.txt", manifest["license_sha256"])
    frames = []
    for name, digest in manifest["files"].items():
        path = directory / name
        verified_bytes(path, digest)
        with wave.open(str(path), "rb") as wav:
            if wav.getnchannels() != 1 or wav.getsampwidth() != 2 or wav.getcomptype() != "NONE":
                raise ValueError(f"{path}: expected uncompressed mono 16-bit PCM")
            data = wav.readframes(wav.getnframes())
        samples = struct.unpack(f"<{len(data) // 2}h", data)
        n = len(samples)
        coeff = [0j] + [
            sum(x * cmath.exp(-2j * math.pi * h * i / n) for i, x in enumerate(samples)) / n
            for h in range(1, CAPS[0] + 1)
        ]
        if abs(coeff[1]) < 1e-6:
            raise ValueError(f"{path}: no fundamental to phase-align")
        shift = -math.pi / 2 - cmath.phase(coeff[1])
        coeff = [c * cmath.exp(1j * h * shift) for h, c in enumerate(coeff)]
        energy = sum(abs(c) ** 2 for c in coeff)
        centroid = sum(h * abs(c) ** 2 for h, c in enumerate(coeff)) / energy
        frames.append((round(centroid, 12), name, coeff))
    if len(frames) != FRAMES:
        raise ValueError(f"expected {FRAMES} AKWF frames, got {len(frames)}")
    frames.sort()
    return [f[2] for f in frames], [f[1] for f in frames]


def generate():
    basis = [[cmath.exp(2j * math.pi * h * i / SIZE) for i in range(SIZE)]
             for h in range(CAPS[0] + 1)]
    imported = [akwf_frames()] + [akwf_frames(SOURCES / family) for family in AKWF_FAMILIES]
    tables = []
    for table in range(len(INFO)):
        frames = ([original(table, f / (FRAMES - 1)) for f in range(FRAMES)]
                  if table < 7 else imported[table - 7][0])
        normalized = []
        for coeff in frames:
            rms = math.sqrt(2 * sum(abs(c) ** 2 for c in coeff))
            if not rms:
                raise ValueError(f"{INFO[table][0]}: silent source")
            normalized.append([c / rms for c in coeff])
        levels = []
        for cap in CAPS:
            levels.append([
                [2 * sum((coeff[h] * basis[h][i]).real for h in range(1, cap + 1))
                 for i in range(SIZE)]
                for coeff in normalized
            ])
        # A single scale for every frame AND level preserves gain while scanning or changing pitch.
        scale = PEAK / max(abs(x) for level in levels for frame in level for x in frame)
        quantized = []
        for level in levels:
            waves = []
            for frame in level:
                values = [round(x * scale) for x in frame]
                waves.append(values + values[:1])
            quantized.append(waves)
        tables.append(quantized)
    return tables, [names for _, names in imported]


def header(tables, source_names):
    flat = [x for table in tables for level in table for frame in level for x in frame]
    digest = hashlib.sha256(struct.pack(f"<{len(flat)}h", *flat)).hexdigest()
    lines = [
        "/* SPDX-License-Identifier: GPL-3.0-only AND CC0-1.0",
        " * Generated by tools/gen_wavetables.py; do not edit.",
        " * Seven original WT Lab tables; seven derived CC0 AKWF morphing tables.",
        " * See assets/wavetables/akwf/manifest.json and LICENSING.md for provenance.",
        f" * Sample bytes: {len(flat) * 2}; SHA256: {digest}",
        *[" * " + INFO[i + 7][0] + " frame order: " + ", ".join(names)
          for i, names in enumerate(source_names)],
        " */",
        "#ifndef WT_DATA_H", "#define WT_DATA_H", '#include "wt_osc.h"', "",
        f"typedef char wt_generated_shape_matches[(WT_TABLE_COUNT == {len(INFO)}u && WT_LEVEL_COUNT == 7u &&",
        "    WT_FRAME_COUNT == 8u && WT_TABLE_SIZE == 256u && WT_PITCH_MAX == 134u &&",
        "    WT_SAMPLE_RATE == 44100u) ? 1 : -1];", "",
        "const wt_table_info_t wt_table_info[WT_TABLE_COUNT] = {",
    ]
    for i, (name, description) in enumerate(INFO):
        source = "AKWF / Kristoffer Karl Axel Ekstrand (CC0-1.0)" if i >= 7 else "WT Lab (GPL-3.0-only)"
        lines.append(f'    {{"{name}", "{description}", "{source}"}},')
    lines += ["};", "", "static const uint32_t WT_PITCH[WT_PITCH_MAX + 1u] = {"]
    pitches = [round(440 * 2 ** ((note - 69) / 12) * 2 ** 32 / 44100)
               for note in range(PITCH_MAX + 1)]
    for i in range(0, len(pitches), 8):
        lines.append("    " + ", ".join(f"{p}u" for p in pitches[i:i + 8]) + ",")
    lines += ["};", "", "static const int16_t WT_WAVES[WT_TABLE_COUNT][WT_LEVEL_COUNT]"
              "[WT_FRAME_COUNT][WT_TABLE_SIZE + 1u] = {"]
    for t, table in enumerate(tables):
        lines.append(f"    {{ /* {INFO[t][0]} */")
        for cap, level in zip(CAPS, table):
            lines.append(f"        {{ /* harmonics 1..{cap} */")
            for frame in level:
                lines.append("            {")
                for i in range(0, len(frame), 16):
                    lines.append("                " + ", ".join(str(x) for x in frame[i:i + 16]) + ",")
                lines.append("            },")
            lines.append("        },")
        lines.append("    },")
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines).encode("ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if checked-in generated data is stale")
    parser.add_argument("output", nargs="?", type=Path, default=OUTPUT)
    args = parser.parse_args()
    data = header(*generate())
    if args.check:
        if not args.output.is_file() or args.output.read_bytes() != data:
            parser.exit(1, "wavetable data is missing or stale; run tools/gen_wavetables.py\n")
        print("wavetable data: reproducible, source hashes verified")
    else:
        args.output.write_bytes(data)
        sample_bytes = len(INFO) * FRAMES * len(CAPS) * (SIZE + 1) * 2
        print(f"wrote {args.output}: {len(INFO)} tables, {FRAMES} frames, {len(CAPS)} levels, {sample_bytes} sample bytes")


if __name__ == "__main__":
    main()
