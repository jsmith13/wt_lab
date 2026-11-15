# SPDX-License-Identifier: GPL-3.0-only
"""Build/link the WT engine with the real JieLi toolchain; no package is produced.

    python3 tests/wt_targetcheck.py
"""
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import build as builder
import gen_wavetables as generator


def main():
    if generator.OUTPUT.read_bytes() != generator.header(*generator.generate()):
        raise SystemExit("wavetable data is stale; run tools/gen_wavetables.py")
    os.environ.setdefault("JIELI_TOOLCHAIN", str(Path.home() / ".jieli" / "toolchain"))
    builder.OUT.mkdir(parents=True, exist_ok=True)
    img, syms, dis, rt = builder.build_app(builder.SRC / "tests" / "wt_target.c", "wt-probe")
    errors, notes = builder.check(img, syms, dis, rt)
    errors += builder.mmio_check()
    for note in notes:
        print("  ok   ", note)
    if errors:
        raise SystemExit("\n".join(errors))
    print("WT target probe: linked and validated; no package generated, no hardware CPU claim")


if __name__ == "__main__":
    main()
