# SPDX-License-Identifier: GPL-3.0-only
"""Verify WT voice fixtures and the playable instrument's audio, panel preview, and MIDI ownership."""
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import wave
import zlib

from wtcheck import fft


def check(ok, text, detail=""):
    print(f"{text}: {'ok' if ok else 'FAIL'} {detail}")
    if not ok:
        raise AssertionError(text)


def read(path, channels):
    with wave.open(str(path), "rb") as wav:
        if wav.getframerate() != 44100 or wav.getnchannels() != channels or wav.getsampwidth() != 2:
            raise ValueError(f"{path}: expected 44.1 kHz 16-bit audio with {channels} channels")
        data = wav.readframes(wav.getnframes())
    return struct.unpack(f"<{len(data) // 2}h", data)[::channels]


def section(data, start, end):
    return data[round(start * 44100):round(end * 44100)]


def rms(samples):
    return math.sqrt(sum(x * x for x in samples) / len(samples))


def frequency(samples):
    crossings = [i - samples[i] / (samples[i + 1] - samples[i])
                 for i in range(len(samples) - 1) if samples[i] <= 0 < samples[i + 1]]
    return 44100 * (len(crossings) - 1) / (crossings[-1] - crossings[0])


def voice_checks(path):
    samples = read(path, 1)
    check(len(samples) == 10 * 44100, "Complete voice audio fixture")
    for i, expected in enumerate((440, 880, 220, 440 * 2**(0.5 / 12))):
        measured = frequency(section(samples, i + 0.2, i + 0.8))
        cents = 1200 * math.log2(measured / expected)
        check(abs(cents) < 2, f"Oscillator tuning fixture {i}", f"{measured:.4f} Hz, {cents:+.3f} cents")
    ratio = rms(section(samples, 4.2, 4.8)) / rms(section(samples, 5.2, 5.8))
    check(abs(20 * math.log10(ratio / (32 / 127))) < 0.1, "Velocity has the intended gain response")
    lp = 20 * math.log10(rms(section(samples, 6.2, 6.8)) / rms(section(samples, 7.2, 7.8)))
    hp = 20 * math.log10(rms(section(samples, 8.2, 8.8)) / rms(section(samples, 9.2, 9.8)))
    check(-17 < lp < -10, "Low-pass attenuates a note above cutoff", f"{lp:.2f} dB")
    check(-29 < hp < -18, "High-pass attenuates a note below cutoff", f"{hp:.2f} dB")


def image(prefix, ms):
    data = prefix.with_name(prefix.name + f"-{ms}.ppm").read_bytes()
    header, pixels = data.split(b"\n", 1)
    check(header.split() == [b"P6", b"240", b"240", b"255"] and len(pixels) == 240*240*3,
          f"Native panel screenshot at {ms} ms")
    return pixels


def panel_checks(prefix):
    samples = read(prefix.with_suffix(".wav"), 2)
    measured = frequency(section(samples, 0.18, 0.32))
    check(abs(1200 * math.log2(measured / 261.6255653)) < 2,
          "Physical key plays C4 in the native WT application", f"{measured:.4f} Hz")
    check(rms(section(samples, 0.6, 0.8)) > rms(section(samples, 0.2, 0.35)) * 1.3,
          "Overlapping keys produce a chord")
    for start, end, name in ((2.2, 2.4, "Key release after page/octave changes"),
                             (3.35, 3.45, "MIDI velocity-zero release"),
                             (4.0, 4.3, "MIDI all-sound-off panic"),
                             (5.35, 5.45, "Channel-filtered MIDI note-off")):
        check(max(abs(x) for x in section(samples, start, end)) == 0, name)
    check(rms(section(samples, 2.6, 3.0)) > 100, "MIDI plays both edited oscillators")
    check(max(abs(x) for x in section(samples, 4.88, 4.91)) == 0, "Other MIDI channels are rejected")
    check(rms(section(samples, 4.94, 5.0)) > 100, "Selected MIDI channel plays")
    log = prefix.with_suffix(".log").read_text()
    midi = [(int(t), int(st, 16), int(note), int(vel))
            for t, st, note, vel in re.findall(r"(\d+) ms  MIDI out ([0-9A-F]+) (\d+) (\d+)", log)]
    check((4600, 0x90, 48, 100) in midi and (5000, 0x80, 48, 0) in midi,
          "Held local note retains its outgoing MIDI channel and octave")
    ons = [x for x in midi if x[1] & 0xF0 == 0x90]
    offs = [x for x in midi if x[1] & 0xF0 == 0x80]
    check(sorted((st & 15, note) for _, st, note, _ in ons) ==
          sorted((st & 15, note) for _, st, note, _ in offs), "Every outgoing key note has a matching note-off")
    graphs = []
    for ms in (350, 1200):
        pixels = image(prefix, ms)
        graphs.append(pixels[64*240*3:200*240*3])
    check(graphs[0] != graphs[1] and any(graphs[0]), "HOME waveform follows live position edits")
    brightness = []
    for start in (0.6, 1.55):
        block = section(samples, start, start + 8192 / 44100)[:8192]
        spectrum = fft([x * (0.5 - 0.5 * math.cos(2 * math.pi * i / 8191)) for i, x in enumerate(block)])
        brightness.append(sum(abs(spectrum[k])**2 for k in range(280, 930)))
    change = 10 * math.log10(brightness[1] / brightness[0])
    check(change > 10, "Position edits change the sound as well as the drawing", f"{change:.1f} dB upper-band rise")


def ui_checks(prefix):
    samples = read(prefix.with_suffix(".wav"), 2)
    check(max(abs(x) for x in section(samples, 0.72, 0.88)) == 0,
          "SEL tap actually mutes the held oscillator")
    check(rms(section(samples, 1.05, 1.1)) > 100, "Second SEL tap restores its unchanged level")
    check(rms(section(samples, 2.55, 2.6)) > 100, "A/B keeps a held note sounding")
    measured = frequency(section(samples, 4.55, 4.63))
    check(abs(1200 * math.log2(measured / 880)) < 10, "GLO transpose retunes an existing held note",
          f"{measured:.3f} Hz")
    quantized = section(samples, 4.82, 4.88)
    check(all(x % 256 == 0 for x in quantized) and len(set(quantized)) > 2,
          "GLO 8-bit output changes the actual audio, not just its label")
    log = prefix.with_suffix(".log").read_text()
    midi = [(int(t), int(st, 16), int(note), int(vel))
            for t, st, note, vel in re.findall(r"(\d+) ms  MIDI out ([0-9A-F]+) (\d+) (\d+)", log)]
    check(midi == [(5500, 0x90, 54, 100), (5500, 0x90, 78, 100),
                   (5800, 0x80, 54, 0), (5800, 0x80, 78, 0)],
          "A/B gestures send no notes; plain HOME shortcut notes release after context/octave changes")
    pictures = {ms: image(prefix, ms) for ms in
                (200, 500, 700, 2300, 2550, 3500, 3750, 3950, 4100, 4300, 4600, 4800)}
    for ms, pixels in pictures.items():
        check(any(pixels[208*240*3:224*240*3]), f"Four-knob legend is visible at {ms} ms")
    check(pictures[200][64*240*3:200*240*3] != pictures[500][64*240*3:200*240*3],
          "Neighboring-frame/live-morph drawing follows edits")
    spectrum = pictures[3500]

    def bar_height(x):
        return sum(spectrum[(y*240+x)*3+2] >= 100 and
                   spectrum[(y*240+x)*3+2] > spectrum[(y*240+x)*3] * 1.5
                   for y in range(64, 200))

    check(max(bar_height(x) for x in range(18, 26)) >
          max(bar_height(x) for x in range(100, 120)) + 30,
          "Actual HOME analyser displays the held 440 Hz fundamental")
    check(pictures[3750][64*240*3:200*240*3] != pictures[3500][64*240*3:200*240*3] and
          pictures[3950][18*240*3:44*240*3] != pictures[4100][18*240*3:44*240*3],
          "Scope differs from spectrum and HOME restores its plain title after LFO controls")


def workflow_checks(prefix, reload):
    samples = read(prefix.with_suffix(".wav"), 2)
    resumed = read(reload.with_suffix(".wav"), 2)
    check(rms(section(samples, 1.55, 1.8)) > 100 and rms(section(resumed, 0.35, 0.6)) > 100,
          "native saved patch plays both before and after a real process restart")
    check(rms(section(samples, 2.5, 2.7)) > 1, "post-release effects produce a real tail")
    with wave.open(str(reload.with_suffix(".wav")), "rb") as wav:
        stereo = struct.unpack(f"<{wav.getnframes()*2}h", wav.readframes(wav.getnframes()))
    check(any(l != r for l, r in zip(stereo[2*14000:2*26000:2], stereo[2*14000+1:2*26000:2])),
          "reloaded chorus/reverb retain their audible stereo routing")
    for start, expected in ((5.24, 261.625565), (5.74, 329.627557), (6.24, 391.995436), (6.74, 523.251131)):
        measured = frequency(section(samples, start, start + 0.045))
        check(abs(1200 * math.log2(measured / expected)) < 3,
              "step grid drives the actual loop pitches", f"{measured:.3f} Hz")
    check(rms(section(samples, 7.65, 7.665)) > 100 and rms(section(samples, 8.0, 8.1)) < 1,
          "REC count-in really sounds a short click, not a held synth note")
    check(max(map(abs, section(samples, 14.0, 14.5))) == 0,
          "stopped phrase and released MIDI arp leave no stuck sound")
    raw = prefix.with_suffix(".flash.bin").read_bytes()

    def record(at, magic, size):
        offset = at - 0xEE000
        ident, length, crc = struct.unpack_from("<III", raw, offset)
        body = raw[offset + size:offset + size + length]
        check(ident == magic and len(body) == length and zlib.crc32(body) == crc,
              "persistent native flash record header, length and checksum")
        return body

    bank = record(0xF4000, 0x32425457, 16)
    check(struct.unpack_from("<IIII", bank) == (1, 172, 32, 0), "versioned native bank schema")
    voice = bank[16 + 31*172:16 + 32*172]
    check(voice[144:160] == bytes((30, 55, 60, 85, 25, 35, 70, 40, 20, 35, 55, 75, 20, 35, 30, 10)) and
          voice[160:172] == b"JNIT WAVE  \0", "last slot preserves the UI-edited full FX chain and name across processes")
    config = record(0xFC000, 0x32535457, 12)
    check(len(config) == 240 and config[43] >= 4, "quiet autosave persists the edited/overdubbed phrase")
    log = prefix.with_suffix(".log").read_text() + reload.with_suffix(".log").read_text()
    midi = [(int(st, 16), int(note), int(vel))
            for st, note, vel in re.findall(r"MIDI out ([0-9A-F]+) (\d+) (\d+)", log)]
    check(midi == [(0x90, 60, 100), (0x80, 60, 0), (0x90, 64, 100), (0x80, 64, 0),
                   (0x90, 67, 100), (0x80, 67, 0), (0x90, 72, 100), (0x80, 72, 0)],
          "incoming bank/program/note messages are never echoed")
    for path, times in ((prefix, (1100, 1450, 1800, 2100, 3300, 5100, 6000, 7800, 11900, 12800, 13050, 14400)),
                        (reload, (200, 600, 1600, 2000))):
        for ms in times:
            pixels = image(path, ms)
            check(any(pixels[208*240*3:224*240*3]), "native workflow four-knob legend is visible")

    executable = prefix.parent / ("wtlab.exe" if os.name == "nt" else "wtlab")
    script = Path(__file__).with_name("wt_reload.txt")
    with tempfile.TemporaryDirectory(dir=prefix.parent) as temp:
        root = Path(temp)
        for size in (0, 512, 0x11001):
            image_path = root / "invalid.flash"
            original = b"\xff" * size
            image_path.write_bytes(original)
            result = subprocess.run([str(executable.resolve()), str(root / "output"),
                                     str(script.resolve()), "0", str(image_path.resolve())], capture_output=True)
            check(result.returncode == 2 and b"Invalid native flash image" in result.stderr and
                  image_path.read_bytes() == original, "invalid-size host image fails without overwriting existing bytes")
        result = subprocess.run([str(executable.resolve()), str(root / "output"),
                                 str(script.resolve()), "0", str(root.resolve())], capture_output=True)
        check(result.returncode == 2 and root.is_dir(), "host image open/read errors are never treated as blank flash")


def stress_checks(prefix):
    samples = read(prefix.with_suffix(".wav"),2)
    check(rms(section(samples,1.9,3.5)) > 100, "combined eight-voice/modulation/filter/effects scene produces actual audio")
    check(max(map(abs,section(samples,4.2,4.4))) == 0, "combined stress scene recovers to silence after panic and FX-off")
    check("MIDI out" not in prefix.with_suffix(".log").read_text(), "stress setup shortcuts and incoming MIDI generate no spurious transmitted notes")
    for ms in (2000,2300,2800,3300,3500):
        image(prefix,ms)


if __name__ == "__main__":
    voice_checks(Path(sys.argv[1]))
    panel_checks(Path(sys.argv[2]))
    ui_checks(Path(sys.argv[3]))
    workflow_checks(Path(sys.argv[4]), Path(sys.argv[5]))
    stress_checks(Path(sys.argv[6]))
    print("WT polyphonic instrument audio/panel checks: all passed")
