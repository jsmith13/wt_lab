#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Hold WT Lab chords for manual app-audio CPU/peak/deadline measurements.

Use GLO bank 4: app CPU, app peak and LATE. SEL resets peak/LATE on the next audio block.
These exclude platform/USB/mastering overhead; host timing is not device timing.

  python tools/load_test.py              # 1, 2, 4, 6, 8 notes, 10 s each
  python tools/load_test.py 8 20 --port NAME
  python tools/load_test.py --dry-run   # no MIDI backend imported or device contacted
Prepare the stress sound in docs/HARDWARE-TESTS.md; start with MASTER low.
"""
import argparse
import math
import sys
import time

NOTES = [48, 52, 55, 60, 64, 67, 72, 76]          # C3 E3 G3 C4 E4 G4 C5 E5


def chord(port, n, secs, channel, velocity, mido):
    print(f"\n=== {n} note{'s' if n != 1 else ''} -- hold, read CPU and PK ===", flush=True)
    for m in NOTES[:n]:
        port.send(mido.Message("note_on", note=m, velocity=velocity, channel=channel))
    time.sleep(secs)
    for m in NOTES[:n]:
        port.send(mido.Message("note_off", note=m, channel=channel))
    print("\r    released", flush=True)
    time.sleep(1.5)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("notes", nargs="?", type=int, choices=range(1,9))
    parser.add_argument("seconds", nargs="?", type=float, default=10)
    parser.add_argument("--channel", type=int, choices=range(1,17), default=1)
    parser.add_argument("--velocity", type=int, choices=range(1,128), default=100)
    parser.add_argument("--port", help="exact MIDI output name; required if multiple FM-1 candidates exist")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    if not math.isfinite(args.seconds) or args.seconds <= 0:
        parser.error("seconds must be finite and positive")
    stages = [(n,args.seconds) for n in ((args.notes,) if args.notes else (1,2,4,6,8))]
    if args.dry_run:
        for n, secs in stages:
            print(f"{n} notes {NOTES[:n]} for {secs:g} s; channel {args.channel}, velocity {args.velocity}")
        return 0
    import mido
    names = mido.get_output_names()
    candidates = [p for p in names if any(s in p.lower() for s in ("fm-1","felucca","wtlab"))]
    if args.port:
        if args.port not in names:
            parser.error(f"output port not found: {args.port}")
        name = args.port
    elif len(candidates) == 1:
        name = candidates[0]
    else:
        parser.error(f"select one FM-1 explicitly with --port; candidates: {candidates}")
    print(f"port: {name}")
    with mido.open_output(name) as port:
        try:
            for n, secs in stages:
                chord(port, n, secs, args.channel-1, args.velocity, mido)
        finally:
            for m in NOTES:                        # never leave a note stuck
                port.send(mido.Message("note_off", note=m, channel=args.channel-1))
            port.send(mido.Message("control_change", control=123, value=0, channel=args.channel-1))
    print("\ndone")


if __name__ == "__main__":
    sys.exit(main())
