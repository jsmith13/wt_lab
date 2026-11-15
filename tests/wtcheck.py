# SPDX-License-Identifier: GPL-3.0-only
"""Verify rendered wavetable audio and generated spectra; see run_wt.ps1 / run_wt.sh."""
import cmath
import hashlib
import math
from pathlib import Path
import struct
import sys
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import gen_wavetables as generator


def fft(samples):
    n = len(samples)
    if n == 1:
        return samples
    even, odd = fft(samples[::2]), fft(samples[1::2])
    twiddles = [cmath.exp(-2j * math.pi * k / n) * odd[k] for k in range(n // 2)]
    return [even[k] + twiddles[k] for k in range(n // 2)] + [
        even[k] - twiddles[k] for k in range(n // 2)]


def read_wave(prefix, suffix):
    with wave.open(str(prefix.with_name(prefix.name + "-" + suffix + ".wav")), "rb") as wav:
        if wav.getframerate() != 44100 or wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise ValueError("expected 44.1 kHz mono 16-bit audio")
        data = wav.readframes(wav.getnframes())
    return struct.unpack(f"<{len(data) // 2}h", data)


def check(message, ok, detail=""):
    print(f"{message:<57} {'ok' if ok else 'FAIL'} {detail}")
    if not ok:
        raise AssertionError(message)


def main(prefix):
    tuning = read_wave(prefix, "tuning")
    for i, note in enumerate((24, 60, 69, 96, 127, 69.5)):
        samples = tuning[i * 22050:(i + 1) * 22050]
        crossings = [j - samples[j] / (samples[j + 1] - samples[j])
                     for j in range(len(samples) - 1) if samples[j] <= 0 < samples[j + 1]]
        measured = 44100 * (len(crossings) - 1) / (crossings[-1] - crossings[0])
        expected = 440 * 2 ** ((note - 69) / 12)
        cents = 1200 * math.log2(measured / expected)
        check(f"MIDI {note:g}: correct pitch", abs(cents) < 1.0, f"{measured:.5f} Hz, {cents:+.3f} cents")

    alias = read_wave(prefix, "alias")
    for i, fundamental in enumerate((7, 137, 589, 1477)):
        spectra = [fft(alias[i * 16384 + j * 8192:i * 16384 + (j + 1) * 8192]) for j in (0, 1)]
        harmonic_bins = set(range(fundamental, 4097, fundamental))
        ratios = []
        for spectrum in spectra:
            wanted = sum(abs(spectrum[k]) ** 2 for k in harmonic_bins)
            unwanted = sum(abs(spectrum[k]) ** 2 for k in range(1, 4097) if k not in harmonic_bins)
            ratios.append(10 * math.log10(wanted / max(unwanted, 1e-30)))
        gain_db = 20 * math.log10(abs(spectra[0][fundamental]) / abs(spectra[1][fundamental]))
        check("Band-limited playback suppresses folded partials",
              ratios[0] > 45 and (i == 0 or ratios[0] - ratios[1] > 35),
              f"{fundamental * 44100 / 8192:.1f} Hz: {ratios[0]:.1f} dB clean, {ratios[1]:.1f} dB naive")
        check("Band limiting preserves the fundamental", abs(gain_db) < 0.1, f"{gain_db:+.3f} dB")

    tables, _ = generator.generate()
    legacy = [sample for table in tables[:8] for level in table for frame in level for sample in frame]
    check("Existing eight tables retain their exact samples and order",
          hashlib.sha256(struct.pack(f"<{len(legacy)}h", *legacy)).hexdigest() ==
          "9c4d08b930a1701b592f57257a9c2ad443219f49d1fc74a99224f0f6d418ac38")
    max_dc = max_outside = max_fundamental_change = 0.0
    for table in tables:
        fundamentals = []
        for level, cap in zip(table, generator.CAPS):
            current = []
            for frame in level:
                check_guard = frame[0] == frame[-1]
                if not check_guard:
                    raise AssertionError("waveform guard sample does not wrap")
                spectrum = fft(frame[:-1])
                max_dc = max(max_dc, abs(spectrum[0]) / generator.SIZE)
                max_outside = max(max_outside, max(
                    (abs(spectrum[h]) / generator.SIZE for h in range(cap + 1, generator.SIZE // 2 + 1)),
                    default=0))
                current.append(abs(spectrum[1]) / generator.SIZE)
            fundamentals.append(current)
        for level in fundamentals[1:]:
            max_fundamental_change = max(max_fundamental_change, max(
                abs(a - b) for a, b in zip(fundamentals[0], level)))
    check("All factory frames have DC removed", max_dc < 0.2, f"{max_dc:.4f} LSB worst")
    check("Every level obeys its harmonic limit", max_outside < 0.2, f"{max_outside:.4f} LSB worst out-of-band bin")
    check("Levels share one gain, not separate normalization", max_fundamental_change < 0.2,
          f"{max_fundamental_change:.4f} LSB worst fundamental change")
    vocal_fundamentals = [fft(frame[:-1])[1] for table in tables[7:] for frame in table[0]]
    check("AKWF frames are aligned to the same fundamental phase",
          all(c.imag < 0 and abs(c.real) / abs(c) < 0.001 for c in vocal_fundamentals))

    demo = read_wave(prefix, "demo")
    check("All scanning tables stay inside headroom",
          len(demo) == len(tables) * 44100 * 2 and max(abs(x) for x in demo) <= 30000)
    brightness = []
    for start in (0.1, 1.7):
        samples = demo[int(start * 44100):int(start * 44100) + 8192]
        windowed = [x * (0.42 - 0.5 * math.cos(2 * math.pi * i / 8191) +
                         0.08 * math.cos(4 * math.pi * i / 8191)) for i, x in enumerate(samples)]
        spectrum = fft(windowed)
        def partial(h):
            center = 130.81278265 * h * 8192 / 44100
            return sum(abs(spectrum[k]) ** 2 for k in range(round(center) - 3, round(center) + 4))
        brightness.append(sum(partial(h) for h in range(2, 9)) / partial(1))
    change = 10 * math.log10(brightness[1] / brightness[0])
    check("Scanning the teaching table audibly adds harmonics", change > 10, f"{change:.1f} dB rise relative to fundamental")
    print("wavetable audio/data checks: all passed")


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build") / "host" / "wavetable")
